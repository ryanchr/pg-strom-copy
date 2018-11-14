create view revenue_tmp (supplier_no, total_revenue) as
	select
		l_suppkey,
		sum(l_extendedprice * (1 - l_discount))
	from
		lineitem
	where l_shipdate >= date '1997-05-01'
		and l_shipdate < date '1997-05-01' + interval '3' month
	group by
		l_suppkey;


select
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	revenue_tmp
where	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			revenue_tmp
	)
order by    s_suppkey
limit 1;

drop view revenue_tmp;
