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
import test.connectPG;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.logging.Level;
import java.util.logging.Logger;


public class loadData{

    public static void loadCSV() throws IOException, FileNotFoundException {

        String url = "jdbc:postgresql://localhost:5432/tpch?socketTimeout=0";
	String user = "postgres";
	String password = "";

        long startTime, endTime;
	
	BufferedReader reader = null;

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
	
        String sqlFilePath = "/home/postgres/pgstrom/tpch_build_db/tpch-build-db";

	try(    Connection con = DriverManager.getConnection(url,user,password);
		Statement st = con.createStatement(); ) {

		for(int i = 1; i < 10; i++) {	
	
		    startTime = System.currentTimeMillis();
	            
		    ResultSet rs = null; 

		    String sqlFile = sqlFilePath + new Integer(i).toString() + ".sql";
		    System.out.println("Running "+sqlFile + "\n");

                    reader = new BufferedReader(new FileReader(sqlFile)); 

		    String line = null;
		    String query = "";

		    while ((line = reader.readLine()) != null) {
		    	query = query + line + " ";
		    }
		    
		    System.out.println(query + "\n");

		    //if(i == 15) st.execute(query); 
		    //else rs = st.executeQuery(query);
		    st.execute(query); 

		    endTime = System.currentTimeMillis();

		    connectPG.printResult(
		        "Query" + String.valueOf(i),
		        (endTime - startTime),
		        8,
		        1
                    );

		    if(rs != null) {
		        while (rs.next()) {
		            System.out.println(rs.getString(1));
		        }
		    }
		}


	} catch (SQLException ex) {
	
		Logger lgr = Logger.getLogger(connectPG.class.getName());
		lgr.log(Level.SEVERE, ex.getMessage(), ex);
	}
    }

}
