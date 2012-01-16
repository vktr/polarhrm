// RCX5_comm.cpp
//
// Copyright (C) 2012 - Thomas Foyth
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <list>
#include "../Datanode.h"

#include "../Driver/driver.h"
#include "../Watch/Overview.h"

#include "RCX5_comm.h"
//#include "RCX5_commands.h"


#include "../util_functions.h"


RCX5comm::RCX5comm(Driver * d){
	this->driver = d;
}



void RCX5comm::setDriver(Driver * d){
	this->driver = d;
}


void RCX5comm::initDriver(void){
	this->driver->init();
}


void RCX5comm::closeDriver(void){
	this->driver->close();
}



void RCX5comm::getUser(unsigned char & raw_user_data, int & len) {

}





// get the watch overview with some basic data
void RCX5comm::getOverview(unsigned char *raw_buffer, int &len) {

	int session_size=0;
	int counter=0;
	unsigned char sendquery[256];


	// first query at an open snyc connection
	// Get overview -> response holds the number of sessions
	// 												report can hold other    0xAA
	unsigned char query[] = {0x01, 0x40, 0x02, 0x00, 0x54, 0x4d, 0x34, 0x1e, 0x00};

	RCX5comm::write_buffer(sendquery, 256, 0);

	//response: 04 42 3c
	unsigned char response[] = {0x04, 0x42, 0x3c};

	memcpy(sendquery, query, sizeof(query));

	printf("get overview\n");
	this->driver->sendbytes(sendquery, sizeof(sendquery));

	do {
		usleep(1000);
		// return the length
		len = this->driver->recvbytes(raw_buffer);

		if( 512 == len 
		&& raw_buffer[0] == response[0]
		&& raw_buffer[1] == response[1]
		&& raw_buffer[2] == response[2]){

			// moved to parsing
			//print_bytes ((char*)raw_buffer, len);

			return;
		}

		counter++;
	}while(counter < 10);

	printf("did not get session size \n");
	exit(-1);
}



// get session information
//
void RCX5comm::getSessionOverview(unsigned char *raw_buffer, int &len, int sess_no) {

	int session_size=0;
	int counter=0;
	unsigned char sendquery[256];
	// FIXME it figured out that the query is not correct
	// ok this is how it goes 
	// this is not a general getOverview requerst , this is a get session size
	// request to get the session size of a spezific session
	// with this information the getsession function is calling for the 
	// itemized packets
	// 																					 0x00
	// 																					 0x01
	unsigned char query[] = {0x01, 0x40, 0x03, 0x00, 0x54, 0x4d, 0x34, 0x1e, 0xb2, 0x00, sess_no };

	RCX5comm::write_buffer(sendquery, 256, 0);

	//response: 04 42 06 00 40 b2 00 5d 28 
	unsigned char response[] = {0x04, 0x42, 0x06};

	memcpy(sendquery, query, sizeof(query));

	printf("get session information\n");
	this->driver->sendbytes(sendquery, sizeof(sendquery));

	do {
		usleep(1000);
		// return the length
		len = this->driver->recvbytes(raw_buffer);

		if( 512 == len 
		&& raw_buffer[0] == response[0]
		&& raw_buffer[1] == response[1]
		&& raw_buffer[2] == response[2]){

			// moved to parsing
			//print_bytes ((char*)raw_buffer, len);

			return;
		}

		counter++;
	}while(counter < 10);

	printf("did not get session size \n");
	exit(-1);
}




/* get the sessiondata out of the watch and store them in a node list. 
 the idea is to have a clear separation and any datamaipulation 
 data are manipulated at the with parse functions */
std::list<Datanode> RCX5comm::getSession(int sess_no, int bytes) {

//get session
//little endian coding
//
//0xBBAA (accumulated  packets size) 
//0xYYXX (packet size)
//eg.
//0x01be
//0x037c
//NN session number?
//                              NN AA BB       XX YY
//01 40 09 00 54 4d 34 1e b3 00 01 00 00 00 00 be 01
//01 40 09 00 54 4d 34 1e b3 00 01 be 01 00 00 be 01
//01 40 09 00 54 4d 34 1e b3 00 01 7c 03 00 00 be 01

	std::list<Datanode> l;

	int checksum = 0;
	int sesschecksum;

	int len,i;
	int nodecount = 0; // counts up
	int nodes_left; // counts down - start value set at first packet

	unsigned char buf[RCX5_TRANSFER_BUFFER_SIZE];
	unsigned char sendquery[256];

	const int packet_size = RCX5_PACKETS_INCLUDING_HEADER;

	union Usize{
		int16_t intvalue;
		unsigned char uchar[2];
	};

	Usize accumulate, packet;


	packet.intvalue = packet_size;
	accumulate.intvalue = 0; // start value

	sesschecksum = bytes;

	if((bytes % packet_size) != 0) {
		// dont forget the modulo as additonal packet
		nodes_left=(bytes/packet_size)+1;
	}
	else {
		nodes_left=(bytes/packet_size);
	}

	printf("%d packets each 446 bytes (load) \n",nodes_left );
	printf("last packet size %d bytes\n",bytes%packet_size);

	// after sending the init query go ahead a long as all bytes get received
	//while (checksum < w_o->used_bytes))
	do {

		// emty buffer
		RCX5comm::write_buffer(sendquery, 255, 0);

		unsigned char query[] = {0x01, 0x40, 0x09, 0x00, 0x54, 0x4d, 0x34, 0x1e, 0xb3, 0x00, 
								 sess_no,
								 accumulate.uchar[0], //LSB least significant byte
								 accumulate.uchar[1], //MSB most significant byte 
								 0x00, 0x00,
								 packet.uchar[0],
								 packet.uchar[1]};

		memcpy(sendquery, query, sizeof(query));

		printf("command:");
		for (int z=0; z < sizeof(query); z++)
			printf("%x ",query[z]);
		printf("\n");

		// send command 
		this->driver->sendbytes(sendquery, sizeof(sendquery));
		do {
			usleep(1000);
			len = this->driver->recvbytes(buf);
		}while (len != 512);


		print_bytes((char*)buf, len);


		printf("There are %i packets left to transfer\n",nodes_left-1);

		// need to subtract the protocol specific bytes
		// but ONLY for checksum.
		//will be managed in parse function
		checksum += len;
		checksum -= RCX5_ALL_OTHER_PACKETS_CORRECTION_HEAD;
		checksum -= RCX5_ALL_OTHER_PACKETS_CORRECTION_TAIL;


		//calculate values for the next query
		accumulate.intvalue += packet_size;
		if(2 == nodes_left ) {
			packet.intvalue = bytes % packet_size;
		}
		else {
			packet.intvalue = packet_size;
		}

		//put the received data into a node list
		Datanode node(buf, len);
		l.push_back(node);
		nodecount++;
		nodes_left--;

		#if defined(DEBUGPRINT)
			printf("get sessiondata bytes: %d / %d\n\n\n",checksum, sesschecksum);
		#endif
	} while (nodes_left > 0);

return l;
}




int RCX5comm::getReminder(unsigned char data[], unsigned char rem_num){
/*
	unsigned char query[1]; 

	query[0]=RCX5_GET_RIMINDER;
	query[1]=rem_num;
	
	this->driver->sendbytes(query, sizeof(query));
	
return driver->recvbytes(data);
*/
}



int RCX5comm::setReminder(unsigned char data[], unsigned char len){


return 0;
}

/*
   disconnect the watch
*/
void RCX5comm::disconnect(void) {
//maybe there is also a command for putting the watch back to watchmode
	this->closeDriver();
}


/*
   delete a session file from the watch
*/
void RCX5comm::deleteFile(unsigned char bcdyear,
                           unsigned char month,
                           unsigned char bcdday,
                           unsigned char bcdhour,
                           unsigned char bcdminute,
                           unsigned char bcdsecond){



}


/*
 delete all sessionfiles from the watch
*/
void RCX5comm::deleteAllFiles(void){


}



//
//
// try to establish a handshake connection between host and watch
//
//
void RCX5comm::handshake(void){

	unsigned char sendquery[256];
	unsigned char rbuf[1024];
	int rlen=0;


	printf("start pairing ...\n");
	RCX5comm::write_buffer(sendquery, 256, 0);

	// handshake call 
	// asking the watch to accept connections with this the given ID
	// query[10-13] bcd handshake ID 0x09, 0x0, 0x04, 0x07  -> ID 9047
	// if handshake id does exists
	// 
	// the following read
	// response is 1 byte long with 0x00 
	// 
	// the read request is sent up to 4 times
	unsigned char q[] = {0x01,0x40,0x06,0x00,0x54,0x4d,0x34,0x1e,0xb6,0x00,0x08,0x00,0x04,0x08};


	// the following read commands 
	// 00
	// 04 40 03 00 40 b6 00 02 
	// 04 42 03 00 40 b6 00 02
	// 04 42 03 00 40 b6 00 02 


	memcpy(sendquery, q, sizeof(q));
	int counter =0;



	this->driver->sendbytes(sendquery, sizeof(sendquery));

	do {
		usleep(1000);
		rlen = this->driver->recvbytes(rbuf);
	}while (rlen <= 1);

	this->idle();

	printf("handshake ? %d\n",rlen);
	print_bytes((char*)rbuf, rlen);


	if(1) {
		printf("sync connection has opend \n");
		usleep(1000);

/*
		// second query at an open snyc connection
		unsigned char q2[]={0x01, 0x40, 0x04, 0x00, 0x54, 0x4d, 0x34, 0x1e, 0xb7, 0x00, 0xff};
		RCX5comm::write_buffer(sendquery, 256, 0);
		memcpy(sendquery, q2, sizeof(q2));

		printf("2 query\n");
		this->driver->sendbytes(sendquery, sizeof(sendquery));
		do {
			usleep(1000);
			rlen = this->driver->recvbytes(rbuf);
		}while (rlen <= 1);
		print_bytes((char*)rbuf, rlen);
*/

		this->idle();

/*
		// no command send before
		// dont know why?
		do {
			usleep(1000);
			rlen = this->driver->recvbytes(rbuf);
		}while (rlen <= 1);
		print_bytes((char*)rbuf, rlen);
*/

		this->idle2();

/*
		// no command send before
		// dont know why?
		do {
			usleep(1000);
			rlen = this->driver->recvbytes(rbuf);
		}while (rlen <= 1);
		print_bytes((char*)rbuf, rlen);



		printf("3 query\n");
		// third query at an open snyc connection
		unsigned char q3[]={0x01, 0x40, 0x02, 0x00, 0x54, 0x4d, 0x34, 0x1e, 0x01};
		RCX5comm::write_buffer(sendquery, 256, 0);
		memcpy(sendquery, q3, sizeof(q3));


		this->driver->sendbytes(sendquery, sizeof(sendquery));
		do {
			usleep(1000);
			rlen = this->driver->recvbytes(rbuf);
		}while (rlen <= 1);
		print_bytes((char*)rbuf, rlen);
*/






	} //end if

}



//
// once the datalnk device is setup, it is looking for HRM watches 
// next to it. the function is periodic reading form the usb 
// waiting to receive data with a protocol format that includes 
// information with the avaible watch.
//
int RCX5comm::findWatch(int retry){

	int counter=0;
	int IDNumber=0;
	unsigned char rbuf[512];
	int rlen=0;
	unsigned char report[] = {0x04, 0x42, 0x20 };

	printf("start searching for a watch ...");

	do {
		usleep(1000);
		rlen = this->driver->recvbytes(rbuf);

		printf("...");

	// (0 == memcmp(rbuf,report, sizeof(report)*3 ))
		if( 512 == rlen 
		&& rbuf[0] == report[0]
		&& rbuf[1] == report[1]
		&& rbuf[2] == report[2]){

			IDNumber  = unbcd(rbuf[21])*100000;
			IDNumber += unbcd(rbuf[22])*10000;
			IDNumber += unbcd(rbuf[23])*1000;
			IDNumber += unbcd(rbuf[24])*100;
			IDNumber += unbcd(rbuf[25])*10;
			IDNumber += unbcd(rbuf[26])*1; 
			printf("\nfound watch with ID number (serial number?) %d\n",IDNumber);
			printf("in the received buffer must be the product name string\n");

			return IDNumber;
		}
		counter++;
	} while(counter < retry );
	printf("\ndid not find any watch!\n");
	return -1;
}




//
// this fuction is a periodic report that appears quite often in the logs
// it looks like a some kind of report to stay connected
//
void RCX5comm::idle(void){

	unsigned char sendquery[256];

	// there is also a command sending 0x55 
	// witch could have a similar purpose
	// unsigned char y[] = {0x01,0x40,0x01,0x00,0x55};
	unsigned char y[] = {0x01,0x40,0x01,0x00,0x51};
	memcpy(sendquery, y, sizeof(y));
	this->driver->sendbytes(sendquery, sizeof(sendquery));
}




//
// this fuction is a periodic report that appears quite often in the logs
// it looks like a some kind of report to stay connected
//
void RCX5comm::idle2(void){

	unsigned char sendquery[256];

	// there is also a command sending 0x55 
	// witch could have a similar purpose
	// unsigned char y[] = {0x01,0x40,0x01,0x00,0x51};
	unsigned char y[] = {0x01,0x40,0x01,0x00,0x55};
	memcpy(sendquery, y, sizeof(y));
	this->driver->sendbytes(sendquery, sizeof(sendquery));
}












const void RCX5comm::write_buffer(unsigned char* buf, int len, unsigned char filler){

	for (int i=0; i<= len; i++) {
		buf[i]= filler;
	}
}