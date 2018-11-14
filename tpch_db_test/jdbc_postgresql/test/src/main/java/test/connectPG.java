//package connectADS;
package test;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.io.OutputStream;

import test.MultiOutputStream;
import test.loadData;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.logging.Level;
import java.util.logging.Logger;


public class connectPG{

    public static String url = "jdbc:postgresql://localhost:5432/tpch?socketTimeout=0";
    public static String user = "postgres";
    public static String password = "";
    public static String sqlFilePath = "/home/postgres/pgstrom/tpch_db_test/pg_tpch_modified_sqls/";

    public static void printResult(String name, long ms, long bytes, int i) {
        System.out.println(
            name + ": " +
            (ms / 1000.0) + "s   \t " +
            Math.round(1000.0 * i / ms) + " queries/second   \t"
        );
    }

    public static void main(String[] args) throws IOException, FileNotFoundException{
	if(args[0].equals("load")) {
	    System.out.println("Now loading data.");
            loadData.loadCSV();
	} else if (args[0].equals("run")) {
            System.out.println("Now running TPCH benchmarks.");
    	    File file_out = new File("stdout.log");
    	    File file_err = new File("stderr.log");
    	   
    	    FileOutputStream fout= new FileOutputStream(file_out);
    	    FileOutputStream ferr = new FileOutputStream(file_err);
    	   
    	    MultiOutputStream multiOut= new MultiOutputStream(System.out, fout);
    	    MultiOutputStream multiErr= new MultiOutputStream(System.err, ferr);
    	   	
    	    PrintStream stdout= new PrintStream(multiOut);
    	    PrintStream stderr= new PrintStream(multiErr);
    	   	
    	    System.setOut(stdout);
    	    System.setErr(stderr);
	    if(args.length >= 2) runTPCH(args[1]);	
	    else runTPCH();
        }
    }
    
    public static String fileToStr(String sqlFile) throws IOException, FileNotFoundException {
	try {
	    BufferedReader reader = null;

	    String line = null;
	    String query = "";

            reader = new BufferedReader(new FileReader(sqlFile)); 

	    while ((line = reader.readLine()) != null) {
		query = query + line + " ";
	    }
	
	    return query;
	} catch (IOException e) {
	    e.printStackTrace();
	    return "";
	}
    }

    public static void runTPCH(String sqlFile) throws IOException, FileNotFoundException {
        long startTime, endTime;

	try(    Connection con = DriverManager.getConnection(url,user,password);
		Statement st = con.createStatement(); ) {
	
	    startTime = System.currentTimeMillis();
	    
	    ResultSet rs = null; 

            sqlFile = sqlFilePath + sqlFile + ".sql";

	    String query = fileToStr(sqlFile);

	    System.out.println("Running " + sqlFile + "\n");
	    
	    System.out.println(query + "\n");

	    st.execute(query); 

	    rs = st.getResultSet();

	    endTime = System.currentTimeMillis();

	    printResult(
		"Query time:" ,
		(endTime - startTime),
		8,
		1
            );

	    while (rs.next()) {
		System.out.println(rs.getString(1));
	    }

	} catch (SQLException ex) {
		Logger lgr = Logger.getLogger(connectPG.class.getName());
		lgr.log(Level.SEVERE, ex.getMessage(), ex);
	}
    }

    public static void runTPCH() throws IOException, FileNotFoundException {
        long startTime, endTime;

	try(    Connection con = DriverManager.getConnection(url,user,password);
		Statement st = con.createStatement(); ) {

		for(int i = 1; i < 15; i++) {	
		    //if( i==15 || i==18 || i==19 || i==20) continue;
	
		    startTime = System.currentTimeMillis();
	            
		    ResultSet rs = null; 

		    String sqlFile = sqlFilePath + new Integer(i).toString() + ".sql";
		    System.out.println("Running "+sqlFile + "\n");

		    String query = fileToStr(sqlFile);
 
		    System.out.println(query + "\n");

	            boolean hasRes = st.execute(query); 

	            if(hasRes) rs = st.getResultSet();

		    endTime = System.currentTimeMillis();

		    connectPG.printResult(
		        "Query" + String.valueOf(i),
		        (endTime - startTime),
		        8,
		        1
                    );

		    while (rs.next()) {
		        System.out.println(rs.getString(1));
		    }
		}


	} catch (SQLException ex) {
	
		Logger lgr = Logger.getLogger(connectPG.class.getName());
		lgr.log(Level.SEVERE, ex.getMessage(), ex);
	}
    }

}
