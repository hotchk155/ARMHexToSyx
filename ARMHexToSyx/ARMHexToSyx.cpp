// ARMHexToSyx.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "stdlib.h"

/*
:020000040000FA
:08000000B731052800000000E3
:0C3E00008A01000000008A018200FB2FF4
:0E3E120022000C110C152000A2012000911EB0
:103E20000E2F230019082000A000F0302005A70065
:103E30002708F03A031D442F2008F83A03190E2FE3
:103E4000023A03190E2F013A03190E2F073A0319EC
:103E50000E2F0C3A0319302F073A0319362F0E2F65
:103E6000A208031D322F0230A2000E2F2208013AB1
:103E700003190E2F013A03190E2F053A031D432F84
:103E8000A30803190800432F80302005A800A901CA
:103E9000A8080319A90AA908031D562F2208003AE9
:103EA00003190E2F013A03190E2F552F2208003A3D
:103EB00003190E2F013A03190E2F033A03196D2F20
:103EC000013A0319762F073A0319802F013A031993
:103ED0008E2F033A0319942F0E2FA008031D732F62
:103EE0000330A2000E2F0130A2000E2F20087F3ACF
:103EF000031D7D2F0430A2000E2F0130A2000E2FD3
:103F00002008123A031D8B2FA3010530A2002200C6
:103F10000C150C150E2F0130A2000E2F200EF039BB
:103F2000A1000630A2000E2F0F302005AA002A089B
:103F3000A1040530A2002308003A0319A92F013A71
:103F40000319AE2F033A0319B32F013A0319B82FFF
:103F50000E2F2108A4000130A3000E2F2108A50078
:103F60000230A3000E2F2108A6000330A3000E2F5D
:103F70003730250203180E2FA508031DCB2F043060
:103F800026020318CB2F373023009200200026088A
:103F900023009100D22F250823009200200026083C
:103FA00023009100951715158B1355309600AA30F4
:103FB0009600951400000000000095102000A40B4E
:103FC000E32FA3010E2FA60A0E2F7A3021009900AD
:103FD00023009E01B0309D0008309F009C011F30DF
:103FE0009B00FB3021008C008E0020008E1D0927D5
:0A3FF0009F31002808009831E52FEA
:020000040001F9
:02000E00A4FF4D
:02001000FFDE11
:00000001FF


OUTPUT FORMAT,
multiple blocks of the following format

0xF0 <id1> <id2> <id3>
<seq>															sequence number 0..127 increments, wraps around
<data0><data1>...<data63>										64 data bytes of 7bits - msb is set to 0
<msb6..0><msb13..7><msb14..20>...
6	0
13	7
20	14
27	21
34	28
41	35
47	42
55	49
62	56
69	63

<checksum>
0xF7

 0123456789
:02001000FFDE11
 ^^-------------- byte count
   ^^^^---------- address
	   ^^-------- record type
		 ^^^^---- data
			 ^^-- checksum
*/

#define SYSEX_START	0xF0
#define SYSEX_ID0	0x00
#define SYSEX_ID1	0x7F
#define SYSEX_END	0xF7

#define MEMORY_SIZE 65536


#define MAX_LINE 1000



typedef unsigned char byte;
byte memory[MEMORY_SIZE];

int product_id = 0;

#define DATA_BYTES_PER_BLOCK 32 
#define BIT7_BYTES_PER_BLOCK 5 //(5 bytes * 7 bits = 35 total bits)


int app_base_addr = 0;

char from_hex(char in) {
	switch (toupper(in)) {
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A': return 10;
	case 'B': return 11;
	case 'C': return 12;
	case 'D': return 13;
	case 'E': return 14;
	case 'F': return 15;
	}
	return 0;
}


bool read_hex(FILE *infile, unsigned int *max_addr)
{
	int line = 0; // line number (for error message)
	int msg_sequence = 0; // sequence number for sysex message
	unsigned int offset = 0;

	// loop through the input file
	while (!feof(infile)) {

		++line;

		// read a line
		char buf[MAX_LINE + 1];
		if (!fgets(buf, MAX_LINE, infile)) {
			break;
		}
		buf[MAX_LINE] = '\0';

		// should start with :
		if (buf[0] != ':') {
			printf("Hex file error - invalid data at line %d\n", line);
			return false;
		}

		// trim off any trailing line delimiters
		int line_len = strlen(buf);
		while (buf[line_len - 1] == '\r' || buf[line_len - 1] == '\n') {
			--line_len;
		}

		// check line is longer than minimum length
		if (line_len < 9) {
			printf("Hex file error - line too short to be valid at %d\n", line);
			return false;
		}

		// check that the declared data length matches the line length
		int data_len = 16 * from_hex(buf[1]) + from_hex(buf[2]);
		if (2 * data_len + 11 != line_len) { // remembering :
			printf("Hex file error - line length does not match data length at line %d\n", line);
			return false;
		}

		// 01 - END OF FILE
		if (buf[7] == '0' && buf[8] == '1') {
			break;
		}
		// 02 - EXTENDED SEGMENT ADDRESS
		else if (buf[7] == '0' && buf[8] == '2') {
			printf("Hex file error - extended segment address (02) record not supported, line %d\n", line);
			return false;
		}
		// 03 - START SEGMENT ADDRESS
		else if (buf[7] == '0' && buf[8] == '3') {
			printf("WARNING - start segment address (03) record not supported, truncate processing at line %d\n", line);
			return true;
		}
		// 04 - EXTENDED LINEAR ADDRESS
		else if (buf[7] == '0' && buf[8] == '4') {
			if (line_len != 15) {
				printf("Hex file error - invalid linear address record line %d\n", line);
				return false;
			}

			// form the offset
			offset = from_hex(buf[9]);
			offset <<= 4;
			offset |= from_hex(buf[10]);
			offset <<= 4;
			offset |= from_hex(buf[11]);
			offset <<= 4;
			offset |= from_hex(buf[12]);
			offset <<= 16;
			printf("-Linear address offset %x from line  %d\n", offset, line);
		}
		// 05 - START LINEAR ADDRESS
		else if (buf[7] == '0' && buf[8] == '5') {
			printf("Hex file error - start segment address (05) record not supported, line %d\n", line);
			return false;
		}
		else if (buf[7] != '0' || buf[8] != '0') {
			printf("Hex file error - unknown record type, line %d\n", line);
			return false;
		}
		// 00 - DATA
		else {

			// form the base address
			unsigned int addr = from_hex(buf[3]);
			addr <<= 4;
			addr |= from_hex(buf[4]);
			addr <<= 4;
			addr |= from_hex(buf[5]);
			addr <<= 4;
			addr |= from_hex(buf[6]);

			addr += offset;

			if (addr < app_base_addr) {
				printf("Warning: address %x out of range (low) at line %d\n", addr, line);
				continue; 
			}
			if (addr + data_len >= MEMORY_SIZE) {
				printf("Warning: address %x out of range (high) at line %d\n", addr, line);
				continue;
			}

			// copy the data into the memory buffer
			int data_pos = 9;
			unsigned int data;
			for (int i = 0; i < data_len; ++i) {
				data = from_hex(buf[data_pos++]);
				data <<= 4;
				data |= from_hex(buf[data_pos++]);
				memory[addr++] = data;
				if (*max_addr < addr) {
					*max_addr = addr;
				}
			}
		}
	}
	return true;
}

bool write_sysex(FILE *outfile, unsigned int max_addr)
{
	int msg_sequence = 1;
	unsigned int addr = app_base_addr;
	while (addr < max_addr) {

		byte csum = 0;

		// write record to sysex
		fputc(SYSEX_START, outfile);		// } start tag
		fputc(SYSEX_ID0, outfile);			// } manufacturer id
		fputc(SYSEX_ID1, outfile);			// }
		fputc(product_id, outfile);			// }
		fputc(msg_sequence, outfile);		// sequence number
		csum ^= msg_sequence;

		// write out every byte minus the top bit
		for (int i = 0; i < DATA_BYTES_PER_BLOCK; ++i) {
			byte ch = (memory[addr + i] & 0x7F);
			fputc(ch, outfile);
			csum ^= ch;
		}
		
		for (int ofs = 0; ofs < DATA_BYTES_PER_BLOCK;) {
			byte acc = 0;
			// process blocks of 7 data bytes
			for (int i = 0; i < 7; i++) {
				acc >>= 1;
				if (ofs < DATA_BYTES_PER_BLOCK) {
					// gather up bit 7 of each of the 7 data bytes in the block
					// and accumulate them into a new 7-bit value with the bit 0
					// holding top bit of first byte, bit 1 holding the top bit of
					// second byte and so on
					byte d = memory[addr + ofs];
					if (d & 0x80) {
						acc |= 0x40;
					}
				}
				++ofs;
			}
			// output the 7 bits 
			fputc(acc, outfile);
			csum ^= acc;
		}
		fputc((csum & 0x7F), outfile);					// } checksum
		fputc(SYSEX_END, outfile);			// } end tag

		// advance the sequence number
		if (++msg_sequence > 0x7F)
			msg_sequence = 1;

		// move along to next block
		addr += DATA_BYTES_PER_BLOCK;
	}

	// End marker
	fputc(SYSEX_START, outfile);
	fputc(SYSEX_ID0, outfile);
	fputc(SYSEX_ID1, outfile);
	fputc(product_id, outfile);
	fputc(0x00, outfile);		// sequence value 0 marks end of data
	for (int i = 0; i < (DATA_BYTES_PER_BLOCK + BIT7_BYTES_PER_BLOCK); ++i) {
		fputc(0x00, outfile);
	}
	fputc(0x00, outfile);
	fputc(SYSEX_END, outfile);

	return true;

}

bool process_file(FILE *infile, FILE *outfile)
{
	// start by clearing all the memory
	memset(memory, 0xFF, sizeof(memory));
	unsigned int max_addr = 0;
	if (!read_hex(infile, &max_addr)) {
		return false;
	}
	return write_sysex(outfile, max_addr);
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Use: hex2syx <id> <input.hex> <output.syx>\n"
			"[a] noodlebox\n"
		);
		exit(1);
	}
	switch (*argv[1]) {
	case 'a':
		printf("noodlebox\n");
		product_id = 0x21;
		app_base_addr = 0xA000;
		break;
	default:
		printf("invalid product id\n");
		exit(2);
	}

	FILE * infile = NULL;	
	if (fopen_s(&infile, argv[2], "rt")) {
		printf("input file not found\n");
		exit(3);
	}
	FILE * outfile = NULL;
	if (fopen_s(&outfile, argv[3], "wb")) {
		printf("cannot open output file\n");
		exit(4);
	}
	if (process_file(infile, outfile)) {
		printf("done\n");
	}
	fclose(outfile);
	fclose(infile);
	return 0;
}

