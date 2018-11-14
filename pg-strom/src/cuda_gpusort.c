const char *pgstrom_cuda_gpusort_code =
  "/*\n"
  " * cuda_gpusort.h\n"
  " *\n"
  " * GPU implementation of GPU bitonic sorting\n"
  " * --\n"
  " * Copyright 2011-2016 (C) KaiGai Kohei <kaigai@kaigai.gr.jp>\n"
  " * Copyright 2014-2016 (C) The PG-Strom Development Team\n"
  " *\n"
  " * This program is free software; you can redistribute it and/or modify\n"
  " * it under the terms of the GNU General Public License version 2 as\n"
  " * published by the Free Software Foundation.\n"
  " *\n"
  " * This program is distributed in the hope that it will be useful,\n"
  " * but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
  " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
  " * GNU General Public License for more details.\n"
  " */\n"
  "#ifndef CUDA_GPUSORT_H\n"
  "#define CUDA_GPUSORT_H\n"
  "\n"
  "/*\n"
  " * GPU Accelerated Sorting\n"
  " *\n"
  " * It packs kern_parambu, status field, and kern_row_map structure\n"
  " * within a continuous memory area, to translate this chunk with\n"
  " * a single DMA call.\n"
  " */\n"
  "typedef struct\n"
  "{\n"
  "\tkern_errorbuf\tkerror;\n"
  "\tcl_uint\t\t\tsegid;\t\t/* segment id to be loaded */\n"
  "\tcl_uint\t\t\tn_loaded;\t/* number of items already loaded */\n"
  "\t/* performance counter */\n"
  "\tstruct {\n"
  "\t\tcl_uint\t\tnum_kern_lsort;\n"
  "\t\tcl_uint\t\tnum_kern_ssort;\n"
  "\t\tcl_uint\t\tnum_kern_msort;\n"
  "\t\tcl_uint\t\tnum_kern_fixvar;\n"
  "\t\tcl_float\ttv_kern_lsort;\n"
  "\t\tcl_float\ttv_kern_ssort;\n"
  "\t\tcl_float\ttv_kern_msort;\n"
  "\t\tcl_float\ttv_kern_fixvar;\n"
  "\t} pfm;\n"
  "\tkern_parambuf\tkparams;\n"
  "\t/* input chunk shall be located just after the kparams*/\n"
  "} kern_gpusort;\n"
  "\n"
  "#define KERN_GPUSORT_PARAMBUF(kgpusort)\t\t\t(&(kgpusort)->kparams)\n"
  "#define KERN_GPUSORT_PARAMBUF_LENGTH(kgpusort)\t((kgpusort)->kparams.length)\n"
  "#define KERN_GPUSORT_KDS_IN(kgpusort)\t\t\t\t\t\t\t\t\\\n"
  "\t((kern_data_store *)((char *)KERN_GPUSORT_PARAMBUF(kgpusort) +\t\\\n"
  "\t\t\t\t\t\t KERN_GPUSORT_PARAMBUF_LENGTH(kgpusort)))\n"
  "#define KERN_GPUSORT_DMASEND_LENGTH(kgpusort)\t\t\\\n"
  "\t(offsetof(kern_gpusort, kparams) +\t\t\t\t\\\n"
  "\t KERN_GPUSORT_PARAMBUF_LENGTH(kgpusort))\n"
  "#define KERN_GPUSORT_DMARECV_LENGTH(kgpusort)\t\t\t\t\t\t\\\n"
  "\toffsetof(kern_gpusort, kparams)\n"
  "\n"
  "/*\n"
  " * NOTE: Persistent segment - GpuSort have two persistent data structure\n"
  " * with longer duration than individual GpuSort tasks.\n"
  " * One is kern_resultbuf, the other is kern_data_store that keeps sorting\n"
  " * key and identifier of the original records.\n"
  " * The KDS can keep variable length fields using extra area, during the\n"
  " * sorting stage. Once bitonic sorting gets completed, extra area shall\n"
  " * be reused to store the identifier of the original records.\n"
  " * Thus, extra area has to have (sizeof(cl_ulong) * kds->nitems) at least.\n"
  " * If KDS tries to growth more than the threshold, it should be canceled.\n"
  " */\n"
  "\n"
  "#ifdef __CUDACC__\n"
  "/*\n"
  " * Sorting key comparison function - to be generated by PG-Strom\n"
  " * on the fly.\n"
  " */\n"
  "STATIC_FUNCTION(cl_int)\n"
  "gpusort_keycomp(kern_context *kcxt,\n"
  "\t\t\t\tkern_data_store *kds_slot,\n"
  "\t\t\t\tsize_t x_index,\n"
  "\t\t\t\tsize_t y_index);\n"
  "\n"
  "/*\n"
  " * gpusort_projection\n"
  " *\n"
  " * It loads all the rows in the supplied chunk. If no space left on the\n"
  " * persistent segment, it tells the host code to switch new segment.\n"
  " */\n"
  "KERNEL_FUNCTION(void)\n"
  "gpusort_projection(kern_gpusort *kgpusort,\n"
  "\t\t\t\t   kern_resultbuf *kresults,\n"
  "\t\t\t\t   kern_data_store *kds_slot,\n"
  "\t\t\t\t   kern_data_store *kds_in)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tkern_tupitem   *tupitem = NULL;\n"
  "\tcl_uint\t\t   *row_index = KERN_DATA_STORE_ROWINDEX(kds_in);\n"
  "\tcl_bool\t\t\ttup_isnull[GPUSORT_DEVICE_PROJECTION_NFIELDS];\n"
  "\tDatum\t\t\ttup_values[GPUSORT_DEVICE_PROJECTION_NFIELDS];\n"
  "\tcl_bool\t\t   *dest_isnull;\n"
  "\tDatum\t\t   *dest_values;\n"
  "\tcl_uint\t\t\textra_len = 0;\n"
  "\tcl_uint\t\t\textra_ofs;\n"
  "\tcl_uint\t\t\textra_sum;\n"
  "\tchar\t\t   *extra_buf;\n"
  "\tchar\t\t   *extra_pos;\n"
  "\tcl_uint\t\t\tnrows_sum;\n"
  "\tcl_uint\t\t\tnrows_ofs;\n"
  "\tcl_uint\t\t\tkds_index;\n"
  "\tcl_uint\t\t\ti, ncols;\n"
  "\t__shared__ cl_uint may_overflow;\n"
  "\t__shared__ cl_uint extra_base;\n"
  "\t__shared__ cl_uint nrows_base;\n"
  "\t__shared__ cl_uint kresults_base;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_projection, kparams);\n"
  "\n"
  "\t/*\n"
  "\t * Fetch the source tuple, if it is valid. Elsewhere, tupitem == NULL.\n"
  "\t */\n"
  "\tif (get_global_id() < kds_in->nitems &&\n"
  "\t\t(row_index[get_global_id()] & 0x01) == 0)\n"
  "\t{\n"
  "\t\ttupitem = (kern_tupitem *)((char *)kds_in +\n"
  "\t\t\t\t\t\t\t\t   row_index[get_global_id()]);\n"
  "\t}\n"
  "\tnrows_ofs = pgstromStairlikeSum(tupitem != NULL ? 1 : 0, &nrows_sum);\n"
  "\n"
  "\t/*\n"
  "\t * Quick bailout if we have no hope for buffer allocation on the current\n"
  "\t * sorting segment, prior to the tuple extraction and buffer allocation\n"
  "\t * on the kds_slot.\n"
  "\t *\n"
  "\t * NOTE: We try to reference kds_slot->{nitems, usage} without atomic\n"
  "\t * guarantee, so it can lead inconsistent view between threads in same\n"
  "\t * block, depending on the concurrent access.\n"
  "\t * Thus, if any of the thread would detect potential overflow, all the\n"
  "\t * threads in this block will escape.\n"
  "\t *\n"
  "\t * Also note that we cannot know exact size of extra_len prior to\n"
  "\t * extraction of the tuple. So, we assume extra_len == 0. Of course,\n"
  "\t * it is very naive estimation.\n"
  "\t */\n"
  "\tif (get_local_id() == 0)\n"
  "\t\tmay_overflow = 0;\n"
  "\t__syncthreads();\n"
  "\n"
  "\tif (KERN_DATA_STORE_SLOT_LENGTH(kds_slot,\n"
  "\t\t\t\t\t\t\t\t\tkds_slot->nitems + nrows_sum) +\n"
  "\t\tkds_slot->usage > kds_slot->length)\n"
  "\t\tmay_overflow = 1;\n"
  "\t__syncthreads();\n"
  "\n"
  "\tif (may_overflow > 0)\n"
  "\t{\n"
  "\t\tSTROM_SET_ERROR(&kcxt.e, StromError_DataStoreNoSpace);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\n"
  "\t/*\n"
  "\t * Extract the sorting keys and put record identifier here.\n"
  "\t * If the least bit of row_index[] (that is usually aligned to 8-bytes)\n"
  "\t * is set, it means this record is already loaded to another sorting\n"
  "\t * segment, so we shall ignore them\n"
  "\t */\n"
  "\tif (tupitem != NULL)\n"
  "\t{\n"
  "\t\textra_len = deform_kern_heaptuple(&kcxt,\n"
  "\t\t\t\t\t\t\t\t\t\t  kds_in,\n"
  "\t\t\t\t\t\t\t\t\t\t  tupitem,\n"
  "\t\t\t\t\t\t\t\t\t\t  kds_slot->ncols,\n"
  "\t\t\t\t\t\t\t\t\t\t  false,\t/* as device pointer */\n"
  "\t\t\t\t\t\t\t\t\t\t  tup_values,\n"
  "\t\t\t\t\t\t\t\t\t\t  tup_isnull);\n"
  "\t\tassert(extra_len == MAXALIGN(extra_len));\n"
  "\t}\n"
  "\t/* consumption of the extra buffer by this block */\n"
  "\textra_ofs = pgstromStairlikeSum(extra_len, &extra_sum);\n"
  "\n"
  "\t/* buffer allocation on the current sorting block, by atomic operations */\n"
  "\tif (get_local_id() == 0)\n"
  "\t{\n"
  "\t\textra_base = atomicAdd(&kds_slot->usage, extra_sum);\n"
  "\t\tnrows_base = atomicAdd(&kds_slot->nitems, nrows_sum);\n"
  "\t}\n"
  "\t__syncthreads();\n"
  "\n"
  "\t/* confirmation of buffer usage */\n"
  "\tif (nrows_base + nrows_sum >= kds_slot->nrooms ||\n"
  "\t\tKERN_DATA_STORE_SLOT_LENGTH(kds_slot,\n"
  "\t\t\t\t\t\t\t\t\tnrows_base + nrows_sum) +\n"
  "\t\t(extra_base + extra_sum) > kds_slot->length)\n"
  "\t{\n"
  "\t\tSTROM_SET_ERROR(&kcxt.e, StromError_DataStoreNoSpace);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\n"
  "\t/* OK, we could get buffer space successfully */\n"
  "\tkds_index = nrows_base + nrows_ofs;\n"
  "\textra_buf = ((char *)kds_slot +\n"
  "\t\t\t\t kds_slot->length - (extra_base + extra_sum) + extra_ofs);\n"
  "\t__syncthreads();\n"
  "\n"
  "\t/*\n"
  "\t * Get space on kresults\n"
  "\t */\n"
  "\tif (get_local_id() == 0)\n"
  "\t\tkresults_base = atomicAdd(&kresults->nitems, nrows_sum);\n"
  "\t__syncthreads();\n"
  "\tassert(kresults_base + nrows_sum < kresults->nrooms);\n"
  "\tif (kresults_base + nrows_sum >= kresults->nrooms)\n"
  "\t{\n"
  "\t\t/*\n"
  "\t\t * It should not happen because kresults->nrooms == kds_slot->nrooms,\n"
  "\t\t * so once we could acquire the slot on kds_slot, kresults shall also\n"
  "\t\t * have enough rooms to store.\n"
  "\t\t */\n"
  "\t\tSTROM_SET_ERROR(&kcxt.e, StromError_DataStoreNoSpace);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\n"
  "\t/* Copy the values/isnull to the sorting segment */\n"
  "\tif (tupitem != NULL)\n"
  "\t{\n"
  "\t\tkresults->results[kresults_base + nrows_ofs] = kds_index;\n"
  "\n"
  "\t\tdest_isnull = KERN_DATA_STORE_ISNULL(kds_slot, kds_index);\n"
  "\t\tdest_values = KERN_DATA_STORE_VALUES(kds_slot, kds_index);\n"
  "\t\textra_pos = extra_buf;\n"
  "\n"
  "\t\tncols = kds_slot->ncols;\n"
  "\t\tfor (i=0; i < ncols; i++)\n"
  "\t\t{\n"
  "\t\t\tkern_colmeta\tcmeta = kds_slot->colmeta[i];\n"
  "\n"
  "\t\t\tif (tup_isnull[i])\n"
  "\t\t\t{\n"
  "\t\t\t\tdest_isnull[i] = true;\n"
  "\t\t\t\tdest_values[i] = (Datum) 0;\n"
  "\t\t\t}\n"
  "\t\t\telse\n"
  "\t\t\t{\n"
  "\t\t\t\tdest_isnull[i] = false;\n"
  "\n"
  "\t\t\t\tif (cmeta.attbyval)\n"
  "\t\t\t\t{\n"
  "\t\t\t\t\t/* fixed length inline variables */\n"
  "\t\t\t\t\tdest_values[i] = tup_values[i];\n"
  "\t\t\t\t}\n"
  "\t\t\t\telse if (cmeta.attlen > 0)\n"
  "\t\t\t\t{\n"
  "\t\t\t\t\t/* fixed length indirect variables */\n"
  "\t\t\t\t\textra_pos = (char *)TYPEALIGN(cmeta.attalign, extra_pos);\n"
  "\t\t\t\t\tassert(extra_pos + cmeta.attlen <= extra_buf + extra_len);\n"
  "\t\t\t\t\tmemcpy(extra_pos,\n"
  "\t\t\t\t\t\t   DatumGetPointer(tup_values[i]),\n"
  "\t\t\t\t\t\t   cmeta.attlen);\n"
  "\t\t\t\t\tdest_values[i] = PointerGetDatum(extra_pos);\n"
  "\t\t\t\t\textra_pos += cmeta.attlen;\n"
  "\t\t\t\t}\n"
  "\t\t\t\telse\n"
  "\t\t\t\t{\n"
  "\t\t\t\t\t/* varlena datum */\n"
  "\t\t\t\t\tcl_uint\t\tvl_len = VARSIZE_ANY(tup_values[i]);\n"
  "\t\t\t\t\textra_pos = (char *)TYPEALIGN(cmeta.attalign, extra_pos);\n"
  "\t\t\t\t\tassert(extra_pos + vl_len <= extra_buf + extra_len);\n"
  "\t\t\t\t\tmemcpy(extra_pos,\n"
  "\t\t\t\t\t\t   DatumGetPointer(tup_values[i]),\n"
  "\t\t\t\t\t\t   vl_len);\n"
  "\t\t\t\t\tdest_values[i] = PointerGetDatum(extra_pos);\n"
  "\t\t\t\t\textra_pos += vl_len;\n"
  "\t\t\t\t}\n"
  "\t\t\t}\n"
  "\t\t}\n"
  "\n"
  "\t\t/*\n"
  "\t\t * Invalidate the row_index, to inform we could successfully move\n"
  "\t\t * this record to kds_slot.\n"
  "\t\t */\n"
  "\t\trow_index[get_global_id()] |= 0x00000001U;\n"
  "\t}\n"
  "\t/* inform host-side the number of rows actually moved */\n"
  "\tif (get_local_id() == 0)\n"
  "\t{\n"
  "\t\tcl_uint hoge =\n"
  "\t\tatomicAdd(&kgpusort->n_loaded, nrows_sum);\n"
  "\t}\n"
  "\t__syncthreads();\n"
  "out:\n"
  "\tkern_writeback_error_status(&kgpusort->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "/*\n"
  " * gpusort_bitonic_local\n"
  " *\n"
  " * It tries to apply each steps of bitonic-sorting until its unitsize\n"
  " * reaches the 2 * workgroup-size (that is expected to power of 2).\n"
  " */\n"
  "KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "gpusort_bitonic_local(kern_gpusort *kgpusort,\n"
  "\t\t\t\t\t  kern_resultbuf *kresults,\n"
  "\t\t\t\t\t  kern_data_store *kds_slot)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tcl_uint\t\t   *localIdx = SHARED_WORKMEM(cl_uint);\n"
  "\tcl_uint\t\t\tlocalLimit;\n"
  "\tcl_uint\t\t\tnitems = kresults->nitems;\n"
  "\tcl_uint\t\t\tpart_size = 2 * get_local_size();\n"
  "\tcl_uint\t\t\tpart_base = get_global_index() * part_size;\n"
  "\tcl_uint\t\t\tblockSize;\n"
  "\tcl_uint\t\t\tunitSize;\n"
  "\tcl_uint\t\t\ti;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_bitonic_local, kparams);\n"
  "\n"
  "\t/* Load index to localIdx[] */\n"
  "\tlocalLimit = (part_base + part_size <= nitems\n"
  "\t\t\t\t  ? part_size\n"
  "\t\t\t\t  : nitems - part_base);\n"
  "\tfor (i = get_local_id(); i < localLimit; i += get_local_size())\n"
  "\t\tlocalIdx[i] = kresults->results[part_base + i];\n"
  "\t__syncthreads();\n"
  "\n"
  "\tfor (blockSize = 2; blockSize <= part_size; blockSize *= 2)\n"
  "\t{\n"
  "\t\tfor (unitSize = blockSize; unitSize >= 2; unitSize /= 2)\n"
  "        {\n"
  "\t\t\tcl_uint\t\tunitMask\t\t= (unitSize - 1);\n"
  "\t\t\tcl_uint\t\thalfUnitSize\t= (unitSize >> 1);\n"
  "\t\t\tcl_uint\t\thalfUnitMask\t= (halfUnitSize - 1);\n"
  "\t\t\tcl_bool\t\treversing\t\t= (unitSize == blockSize);\n"
  "\t\t\tcl_uint\t\tidx0, idx1;\n"
  "\n"
  "\t\t\tidx0 = (((get_local_id() & ~halfUnitMask) << 1) +\n"
  "\t\t\t\t\t(get_local_id() & halfUnitMask));\n"
  "         \tidx1 = ((reversing == true)\n"
  "\t\t\t\t\t? ((idx0 & ~unitMask) | (~idx0 & unitMask))\n"
  "\t\t\t\t\t: (halfUnitSize + idx0));\n"
  "            if(idx1 < localLimit)\n"
  "\t\t\t{\n"
  "\t\t\t\tcl_uint\t\tpos0 = localIdx[idx0];\n"
  "\t\t\t\tcl_uint\t\tpos1 = localIdx[idx1];\n"
  "\n"
  "\t\t\t\tif (gpusort_keycomp(&kcxt, kds_slot, pos0, pos1) > 0)\n"
  "\t\t\t\t{\n"
  "\t\t\t\t\t/* swap them */\n"
  "\t\t\t\t\tlocalIdx[idx0] = pos1;\n"
  "\t\t\t\t\tlocalIdx[idx1] = pos0;\n"
  "\t\t\t\t}\n"
  "\t\t\t}\n"
  "\t\t\t__syncthreads();\n"
  "\t\t}\n"
  "\t}\n"
  "\t/* write back local sorted result */\n"
  "\tfor (i = get_local_id(); i < localLimit; i += get_local_size())\n"
  "\t\tkresults->results[part_base + i] = localIdx[i];\n"
  "\t__syncthreads();\n"
  "\n"
  "\t/* any error during run-time? */\n"
  "\tkern_writeback_error_status(&kresults->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "/*\n"
  " * gpusort_bitonic_step\n"
  " *\n"
  " * It tries to apply individual steps of bitonic-sorting for each step,\n"
  " * but does not have restriction of workgroup size. The host code has to\n"
  " * control synchronization of each step not to overrun.\n"
  " */\n"
  "KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "gpusort_bitonic_step(kern_gpusort *kgpusort,\n"
  "\t\t\t\t\t kern_resultbuf *kresults,\n"
  "\t\t\t\t\t kern_data_store *kds_slot,\n"
  "\t\t\t\t\t size_t unitsz,\n"
  "\t\t\t\t\t cl_bool reversing)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tcl_uint\t\t\tnitems = kresults->nitems;\n"
  "\tcl_uint\t\t\thalfUnitSize = unitsz >> 1;\n"
  "\tcl_uint\t\t\thalfUnitMask = halfUnitSize - 1;\n"
  "\tcl_uint\t\t\tunitMask = unitsz - 1;\n"
  "\tcl_uint\t\t\tidx0, idx1;\n"
  "\tcl_uint\t\t\tpos0, pos1;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_bitonic_step, kparams);\n"
  "\n"
  "\tidx0 = (((get_global_id() & ~halfUnitMask) << 1)\n"
  "\t\t\t+ (get_global_id() & halfUnitMask));\n"
  "\tidx1 = (reversing\n"
  "\t\t\t? ((idx0 & ~unitMask) | (~idx0 & unitMask))\n"
  "\t\t\t: (idx0 + halfUnitSize));\n"
  "\tif (idx1 < nitems)\n"
  "\t{\n"
  "\t\tpos0 = kresults->results[idx0];\n"
  "\t\tpos1 = kresults->results[idx1];\n"
  "\t\tif (gpusort_keycomp(&kcxt, kds_slot, pos0, pos1) > 0)\n"
  "\t\t{\n"
  "\t\t\t/* swap them */\n"
  "\t\t\tkresults->results[idx0] = pos1;\n"
  "\t\t\tkresults->results[idx1] = pos0;\n"
  "\t\t}\n"
  "\t}\n"
  "\tkern_writeback_error_status(&kresults->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "/*\n"
  " * gpusort_bitonic_merge\n"
  " *\n"
  " * It handles the merging step of bitonic-sorting if unitsize becomes less\n"
  " * than or equal to the workgroup size.\n"
  " */\n"
  "KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "gpusort_bitonic_merge(kern_gpusort *kgpusort,\n"
  "\t\t\t\t\t  kern_resultbuf *kresults,\n"
  "\t\t\t\t\t  kern_data_store *kds_slot)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tcl_int\t\t   *localIdx = SHARED_WORKMEM(cl_int);\n"
  "\tcl_uint\t\t\tlocalLimit;\n"
  "\tcl_uint\t\t\tnitems = kresults->nitems;\n"
  "\tcl_uint\t\t\tpart_size = 2 * get_local_size();\n"
  "\tcl_uint\t\t\tpart_base = get_global_index() * part_size;\n"
  "\tcl_uint\t\t\tblockSize = part_size;\n"
  "\tcl_uint\t\t\tunitSize;\n"
  "\tcl_uint\t\t\ti;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_bitonic_merge, kparams);\n"
  "\n"
  "\t/* Load index to localIdx[] */\n"
  "\tlocalLimit = (part_base + part_size <= nitems\n"
  "\t\t\t\t  ? part_size\n"
  "\t\t\t\t  : nitems - part_base);\n"
  "\tfor (i = get_local_id(); i < localLimit; i += get_local_size())\n"
  "\t\tlocalIdx[i] = kresults->results[part_base + i];\n"
  "\t__syncthreads();\n"
  "\n"
  "\t/* merge two sorted blocks */\n"
  "\tfor (unitSize = blockSize; unitSize >= 2; unitSize >>= 1)\n"
  "\t{\n"
  "\t\tcl_uint\t\thalfUnitSize = (unitSize >> 1);\n"
  "\t\tcl_uint\t\thalfUnitMask = (halfUnitSize - 1);\n"
  "\t\tcl_uint\t\tidx0, idx1;\n"
  "\n"
  "\t\tidx0 = (((get_local_id() & ~halfUnitMask) << 1)\n"
  "\t\t\t\t+ (get_local_id() & halfUnitMask));\n"
  "\t\tidx1 = halfUnitSize + idx0;\n"
  "\n"
  "        if (idx1 < localLimit)\n"
  "\t\t{\n"
  "\t\t\tcl_uint\t\tpos0 = localIdx[idx0];\n"
  "\t\t\tcl_uint\t\tpos1 = localIdx[idx1];\n"
  "\n"
  "\t\t\tif (gpusort_keycomp(&kcxt, kds_slot, pos0, pos1) > 0)\n"
  "\t\t\t{\n"
  "\t\t\t\t/* swap them */\n"
  "\t\t\t\tlocalIdx[idx0] = pos1;\n"
  "                localIdx[idx1] = pos0;\n"
  "\t\t\t}\n"
  "\t\t}\n"
  "\t\t__syncthreads();\n"
  "\t}\n"
  "\t/* Save index to kresults[] */\n"
  "\tfor (i = get_local_id(); i < localLimit; i += get_local_size())\n"
  "\t\tkresults->results[part_base + i] = localIdx[i];\n"
  "\t__syncthreads();\n"
  "\n"
  "\tkern_writeback_error_status(&kresults->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "KERNEL_FUNCTION(void)\n"
  "gpusort_fixup_pointers(kern_gpusort *kgpusort,\n"
  "\t\t\t\t\t   kern_resultbuf *kresults,\n"
  "\t\t\t\t\t   kern_data_store *kds_slot)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tcl_uint\t\t\tkds_index;\n"
  "\tDatum\t\t   *tup_values;\n"
  "\tcl_bool\t\t   *tup_isnull;\n"
  "\tcl_int\t\t\ti;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_fixup_pointers, kparams);\n"
  "\n"
  "\tif (get_global_id() < kresults->nitems)\n"
  "\t{\n"
  "\t\tkds_index = kresults->results[get_global_id()];\n"
  "\t\tassert(kds_index < kds_slot->nitems);\n"
  "\n"
  "\t\ttup_values = KERN_DATA_STORE_VALUES(kds_slot, kds_index);\n"
  "\t\ttup_isnull = KERN_DATA_STORE_ISNULL(kds_slot, kds_index);\n"
  "\n"
  "\t\tfor (i=0; i < kds_slot->ncols; i++)\n"
  "\t\t{\n"
  "\t\t\tkern_colmeta\tcmeta = kds_slot->colmeta[i];\n"
  "\n"
  "\t\t\tif (cmeta.attbyval)\n"
  "\t\t\t\tcontinue;\n"
  "\t\t\tif (tup_isnull[i])\n"
  "\t\t\t\tcontinue;\n"
  "\t\t\ttup_values[i] = ((hostptr_t)tup_values[i] -\n"
  "\t\t\t\t\t\t\t (hostptr_t)&kds_slot->hostptr +\n"
  "\t\t\t\t\t\t\t kds_slot->hostptr);\n"
  "\t\t}\n"
  "\t}\n"
  "\tkern_writeback_error_status(&kgpusort->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "KERNEL_FUNCTION(void)\n"
  "gpusort_main(kern_gpusort *kgpusort,\n"
  "\t\t\t kern_resultbuf *kresults,\n"
  "\t\t\t kern_data_store *kds_slot)\n"
  "{\n"
  "\tkern_parambuf  *kparams = KERN_GPUSORT_PARAMBUF(kgpusort);\n"
  "\tkern_context\tkcxt;\n"
  "\tconst void\t   *kern_funcs[3];\n"
  "\tvoid\t\t  **kern_args;\n"
  "\tcl_uint\t\t\tnitems = kresults->nitems;\n"
  "\tcl_uint\t\t\tnhalf;\n"
  "\tcl_uint\t\t\t__block_sz = UINT_MAX;\n"
  "\tdim3\t\t\tgrid_sz;\n"
  "\tdim3\t\t\tblock_sz;\n"
  "\tcl_uint\t\t\ti, j;\n"
  "\tcl_ulong\t\ttv_start;\n"
  "\tcudaError_t\t\tstatus = cudaSuccess;\n"
  "\n"
  "\tINIT_KERNEL_CONTEXT(&kcxt, gpusort_main, kparams);\n"
  "\n"
  "\t/*\n"
  "\t * NOTE: Because of the bitonic sorting algorithm characteristics,\n"
  "\t * block size has to be 2^N value and common in the three kernel\n"
  "\t * functions below, thus we pick up the least one. Usually it shall\n"
  "\t * be hardware's maximum available block size because kernel functions\n"
  "\t * are declared with KERNEL_FUNCTION_MAXTHREADS.\n"
  "\t */\n"
  "\tkern_funcs[0] = (const void *)gpusort_bitonic_local;\n"
  "\tkern_funcs[1] = (const void *)gpusort_bitonic_step;\n"
  "\tkern_funcs[2] = (const void *)gpusort_bitonic_merge;\n"
  "\tfor (i=0; i < 3; i++)\n"
  "\t{\n"
  "\t\tstatus = largest_workgroup_size(&grid_sz,\n"
  "\t\t\t\t\t\t\t\t\t\t&block_sz,\n"
  "\t\t\t\t\t\t\t\t\t\tkern_funcs[i],\n"
  "\t\t\t\t\t\t\t\t\t\t(nitems + 1) / 2,\n"
  "\t\t\t\t\t\t\t\t\t\t0, 2 * sizeof(cl_uint));\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\t\t__block_sz = Min(__block_sz, 1 << (get_next_log2(block_sz.x + 1) - 1));\n"
  "\t}\n"
  "\tassert((__block_sz & (__block_sz - 1)) == 0);\t/* to be 2^N */\n"
  "\tblock_sz.x = __block_sz;\n"
  "\tblock_sz.y = 1;\n"
  "\tblock_sz.z = 1;\n"
  "\n"
  "\t/* nhalf is the least power of two value that is larger than or\n"
  "\t * equal to half of the nitems. */\n"
  "\tnhalf = 1UL << (get_next_log2(nitems + 1) - 1);\n"
  "\n"
  "\t/*\n"
  "\t * makea a sorting block up to (2 * __block_sz)\n"
  "\t *\n"
  "\t * KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "\t * gpusort_bitonic_local(kern_gpusort *kgpusort,\n"
  "\t *                       kern_resultbuf *kresults,\n"
  "\t *                       kern_data_store *kds_slot)\n"
  "\t */\n"
  "\ttv_start = GlobalTimer();\n"
  "\tkern_args = (void **)\n"
  "\t\tcudaGetParameterBuffer(sizeof(void *),\n"
  "\t\t\t\t\t\t\t   sizeof(void *) * 3);\n"
  "\tif (!kern_args)\n"
  "\t{\n"
  "\t\tSTROM_SET_ERROR(&kcxt.e, StromError_OutOfKernelArgs);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\tkern_args[0] = kgpusort;\n"
  "\tkern_args[1] = kresults;\n"
  "\tkern_args[2] = kds_slot;\n"
  "\n"
  "\tgrid_sz.x = ((nitems + 1) / 2 + block_sz.x - 1) / block_sz.x;\n"
  "\tgrid_sz.y = 1;\n"
  "\tgrid_sz.z = 1;\n"
  "\tstatus = cudaLaunchDevice((void *)gpusort_bitonic_local,\n"
  "\t\t\t\t\t\t\t  kern_args, grid_sz, block_sz,\n"
  "\t\t\t\t\t\t\t  2 * sizeof(cl_uint) * block_sz.x,\n"
  "\t\t\t\t\t\t\t  NULL);\n"
  "\tif (status != cudaSuccess)\n"
  "\t{\n"
  "\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\n"
  "\tstatus = cudaDeviceSynchronize();\n"
  "\tif (status != cudaSuccess)\n"
  "\t{\n"
  "\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\tgoto out;\n"
  "\t}\n"
  "\tTIMEVAL_RECORD(kgpusort,kern_lsort,tv_start);\n"
  "\n"
  "\t/* inter blocks bitonic sorting */\n"
  "\tfor (i = block_sz.x; i < nhalf; i *= 2)\n"
  "\t{\n"
  "\t\tfor (j = 2 * i; j > block_sz.x; j /= 2)\n"
  "\t\t{\n"
  "\t\t\t/*\n"
  "\t\t\t * KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "\t\t\t * gpusort_bitonic_step(kern_gpusort *kgpusort,\n"
  "\t\t\t *                       kern_resultbuf *kresults,\n"
  "\t\t\t *                       kern_data_store *kds_slot)\n"
  "\t\t\t *                       size_t unitsz,\n"
  "\t\t\t *                       cl_bool reversing)\n"
  "\t\t\t */\n"
  "\t\t\tsize_t\t\tunitsz = 2 * j;\n"
  "\t\t\tcl_bool\t\treversing = ((j == 2 * i) ? true : false);\n"
  "\t\t\tsize_t\t\twork_size;\n"
  "\n"
  "\t\t\ttv_start = GlobalTimer();\n"
  "\t\t\tkern_args = (void **)\n"
  "\t\t\t\tcudaGetParameterBuffer(sizeof(void *),\n"
  "\t\t\t\t\t\t\t\t\t   sizeof(void *) * 5);\n"
  "\t\t\tif (!kern_args)\n"
  "\t\t\t{\n"
  "\t\t\t\tSTROM_SET_ERROR(&kcxt.e, StromError_OutOfKernelArgs);\n"
  "\t\t\t\tgoto out;\n"
  "\t\t\t}\n"
  "\t\t\tkern_args[0] = kgpusort;\n"
  "\t\t\tkern_args[1] = kresults;\n"
  "\t\t\tkern_args[2] = kds_slot;\n"
  "\t\t\tkern_args[3] = (void *)unitsz;\n"
  "\t\t\tkern_args[4] = (void *)reversing;\n"
  "\n"
  "\t\t\twork_size = (((nitems + unitsz - 1) / unitsz) * unitsz / 2);\n"
  "\t\t\tgrid_sz.x = (work_size + block_sz.x - 1) / block_sz.x;\n"
  "\t\t\tgrid_sz.y = 1;\n"
  "\t\t\tgrid_sz.z = 1;\n"
  "\n"
  "\t\t\tstatus = cudaLaunchDevice((void *)gpusort_bitonic_step,\n"
  "\t\t\t\t\t\t\t\t\t  kern_args, grid_sz, block_sz,\n"
  "\t\t\t\t\t\t\t\t\t  2 * sizeof(cl_uint) * block_sz.x,\n"
  "\t\t\t\t\t\t\t\t\t  NULL);\n"
  "\t\t\tif (status != cudaSuccess)\n"
  "\t\t\t{\n"
  "\t\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\t\tgoto out;\n"
  "\t\t\t}\n"
  "\n"
  "\t\t\tstatus = cudaDeviceSynchronize();\n"
  "\t\t\tif (status != cudaSuccess)\n"
  "\t\t\t{\n"
  "\t\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\t\tgoto out;\n"
  "\t\t\t}\n"
  "\t\t\tTIMEVAL_RECORD(kgpusort,kern_ssort,tv_start);\n"
  "\t\t}\n"
  "\n"
  "\t\t/*\n"
  "\t\t * KERNEL_FUNCTION_MAXTHREADS(void)\n"
  "\t\t * gpusort_bitonic_merge(kern_gpusort *kgpusort,\n"
  "\t\t *                       kern_resultbuf *kresults,\n"
  "\t\t *                       kern_data_store *kds_slot)\n"
  "\t\t */\n"
  "\t\ttv_start = GlobalTimer();\n"
  "\t\tkern_args = (void **)\n"
  "\t\t\tcudaGetParameterBuffer(sizeof(void *),\n"
  "\t\t\t\t\t\t\t\t   sizeof(void *) * 3);\n"
  "\t\tif (!kern_args)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_ERROR(&kcxt.e, StromError_OutOfKernelArgs);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\t\tkern_args[0] = kgpusort;\n"
  "\t\tkern_args[1] = kresults;\n"
  "\t\tkern_args[2] = kds_slot;\n"
  "\n"
  "\t\tgrid_sz.x = ((nitems + 1) / 2 + block_sz.x - 1) / block_sz.x;\n"
  "\t\tgrid_sz.y = 1;\n"
  "\t\tgrid_sz.z = 1;\n"
  "\n"
  "\t\tstatus = cudaLaunchDevice((void *)gpusort_bitonic_merge,\n"
  "\t\t\t\t\t\t\t\t  kern_args, grid_sz, block_sz,\n"
  "\t\t\t\t\t\t\t\t  2 * sizeof(cl_uint) * block_sz.x,\n"
  "\t\t\t\t\t\t\t\t  NULL);\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\n"
  "\t\tstatus = cudaDeviceSynchronize();\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\t\tTIMEVAL_RECORD(kgpusort,kern_msort,tv_start);\n"
  "\t}\n"
  "\n"
  "\t/*\n"
  "\t * If kds_slot contains any attribute of pointer reference, we have to\n"
  "\t * fix up device pointer to host pointer, prior to receive DMA.\n"
  "\t */\n"
  "\tif (kds_slot->has_notbyval)\n"
  "\t{\n"
  "\t\t/*\n"
  "\t\t * KERNEL_FUNCTION(void)\n"
  "\t\t * gpusort_fixup_pointers(kern_gpusort *kgpusort,\n"
  "\t\t *                        kern_resultbuf *kresults,\n"
  "\t\t *                        kern_data_store *kds_slot)\n"
  "\t\t */\n"
  "\t\ttv_start = GlobalTimer();\n"
  "\t\tkern_args = (void **)\n"
  "\t\t\tcudaGetParameterBuffer(sizeof(void *),\n"
  "\t\t\t\t\t\t\t\t   sizeof(void *) * 3);\n"
  "\t\tif (!kern_args)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_ERROR(&kcxt.e, StromError_OutOfKernelArgs);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\t\tkern_args[0] = kgpusort;\n"
  "\t\tkern_args[1] = kresults;\n"
  "\t\tkern_args[2] = kds_slot;\n"
  "\n"
  "\t\tstatus = optimal_workgroup_size(&grid_sz,\n"
  "\t\t\t\t\t\t\t\t\t\t&block_sz,\n"
  "\t\t\t\t\t\t\t\t\t\t(const void *)\n"
  "\t\t\t\t\t\t\t\t\t\tgpusort_fixup_pointers,\n"
  "\t\t\t\t\t\t\t\t\t\tkresults->nitems,\n"
  "\t\t\t\t\t\t\t\t\t\t0, sizeof(cl_uint));\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\n"
  "\t\tstatus = cudaLaunchDevice((void *)gpusort_fixup_pointers,\n"
  "\t\t\t\t\t\t\t\t  kern_args, grid_sz, block_sz,\n"
  "\t\t\t\t\t\t\t\t  sizeof(cl_uint) * block_sz.x,\n"
  "\t\t\t\t\t\t\t\t  NULL);\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\n"
  "\t\tstatus = cudaDeviceSynchronize();\n"
  "\t\tif (status != cudaSuccess)\n"
  "\t\t{\n"
  "\t\t\tSTROM_SET_RUNTIME_ERROR(&kcxt.e, status);\n"
  "\t\t\tgoto out;\n"
  "\t\t}\n"
  "\t\tTIMEVAL_RECORD(kgpusort,kern_fixvar,tv_start);\n"
  "\t}\n"
  "out:\n"
  "\tkern_writeback_error_status(&kresults->kerror, kcxt.e);\n"
  "}\n"
  "\n"
  "#endif\t/* __CUDACC__ */\n"
  "#endif\t/* CUDA_GPUSORT_H */\n"
;
