main : client server

client :
	gcc -Wall src/client.c -o clientTFTP

server :
	gcc -Wall src/serverTFTP.c -o serverTFTP

clean:
	rm *.client
	rm *.server