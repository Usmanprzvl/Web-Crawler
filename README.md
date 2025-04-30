<h1>Simple Web Crawler</h1>

<p>This program take 3 URLs from the user which are valid and returns the html content in the terminal
</p>

<h3>How to run the program<h3>
<P><b>To compile and run the program in linux OS  We follow these steps:<b> <br><br>
1.We Ensure all required libraries that are installed on our system.<br>
2. Compile the program using the following command:
            gcc -o pr crawler.c -lpthread -lcurl -lsqlite3
3. Run the compiled program using:
              ./pr<br><br><p>

Libraries:

	pthread for multithreading.
	curl for HTTP requests.
	sqlite3 for database management.
   

