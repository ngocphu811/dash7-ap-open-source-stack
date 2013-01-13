/*
 *  Created on: Jan 11, 2013
 *  Authors:
 *  	alexanderhoet@gmail.com
 */

#include <cstring>
#include <stdint.h>

#include "fec.h"

const uint8_t fec_lut[16] = {0, 3, 1, 2, 3, 0, 2, 1 , 3 , 0, 2, 1, 0, 3, 1, 2};

//TODO bitfields?
typedef struct {
	uint8_t populated;
	uint8_t metric;


} state;

/*
 * output array size must at least be 2 x length and a multiple of 4bytes
 * TODO append trellis terminator
 */
void conv_encode(uint8_t* input, uint8_t* output, uint16_t length, uint8_t* state)
{
	uint16_t i;
	register uint8_t state_tmp;
	register uint8_t input_tmp;
	register uint16_t output_tmp;

	state_tmp = *state;

	for (i = 0; i < length; i++) {
		input_tmp = input[i];

		//bit 7
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 7) & 0x01;
		output_tmp = fec_lut[state_tmp] << 14;

		//bit 6
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 6) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 12;

		//bit 5
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 5) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 10;

		//bit 4
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 4) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 8;

		//bit 3
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 3) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 6;

		//bit 2
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 2) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 4;

		//bit 1
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= (input_tmp >> 1) & 0x01;
		output_tmp |= fec_lut[state_tmp] << 2;

		//bit 0
		state_tmp = (state_tmp << 1) & 0x0E;
		state_tmp |= input_tmp & 0x01;
		output_tmp |= fec_lut[state_tmp];

		output[i << 1] = output_tmp >> 8;
		output[(i << 1) + 1] = output_tmp;
	}

	*state = state_tmp;
}

/*
 * Length must be a multiple of two
 */
void conv_decode(uint8_t* input, uint8_t* output, uint16_t length)
{
	uint16_t i;
	int8_t j;
	uint8_t k;
	uint8_t symbol;
	uint8_t state_tmp;
	uint8_t symbol_tmp;
	uint8_t metric_tmp;

	state states1[8];
	state states2[8];

	state* states_old;
	state* states_new;
	state* states_tmp;

	uint16_t input_tmp;

	//Initialization
	states_old = states1;
	states_new = states2;
	memset(states_old, 0, sizeof(state)  << 3);
	memset(states_new, 0, sizeof(state)  << 3);
	states_old[0].populated = 1;


	//For every 2 bytes of the input buffer
	for (i = 0; i < length; i+=2) {
		input_tmp = (input[i] << 8) | input[i+1];

		//For every input symbol (left to right)
		for (j = 14; j >= 0; j-=2) {
			symbol = (input_tmp >> j) & 0x03;

			//Start of Viterbi algorithm
			//For every state
			for (k = 0; k < 8; k++) {
				if(states_old[k].populated) {
					//Calculate state and cost for 0 (cost is hamming distance)
					state_tmp = (k << 1) & 0x0E;
					symbol_tmp = fec_lut[state_tmp];
					metric_tmp = states_old[k].metric;
					metric_tmp += ((symbol ^ symbol_tmp) + 1) >> 1;

					//Update new state
					state_tmp &= 0x07;
					if (!states_new[state_tmp].populated || metric_tmp < states_new[state_tmp].metric) {
						states_new[state_tmp].populated = 1;
						states_new[state_tmp].metric = metric_tmp;
					}

					//Calculate state and symbol for 1
					state_tmp = ((k << 1) & 0x0E) | 0x01;
					symbol_tmp = fec_lut[state_tmp];
					metric_tmp = states_old[k].metric;
					metric_tmp += ((symbol ^ symbol_tmp) + 1) >> 1;

					//Update new state
					state_tmp &= 0x07;
					if (!states_new[state_tmp].populated || metric_tmp < states_new[state_tmp].metric) {
						states_new[state_tmp].populated = 1;
						states_new[state_tmp].metric = metric_tmp;
					}
				}
			}

			//Switch state arrays
			states_tmp = states_new;
			states_new = states_old;
			states_old = states_tmp;

			//Reset new state array
			memset(states_new, 0, sizeof(state)  << 3);
		}
	}
}

/*
 * buffer size must be a multiple of 4
 * input and output buffer may be the same
 */
void interleave_deinterleave(uint8_t* input, uint8_t* output, uint16_t length)
{
	uint16_t i;
	register uint16_t input0;
	register uint16_t input1;
	register uint16_t output0;
	register uint16_t output1;

	for (i = 0; i < length; i+=4) {
		input0 = (input[i] << 8) | input[i+1];
		input1 = (input[i+2] << 8) | input[i+3];

		output0 = (input0 >> 10) & 0x03;
		output0 |= ((input0 >> 2) & 0x03) << 2;
		output0 |= ((input1 >> 10) & 0x03) << 4;
		output0 |= ((input1 >> 2) & 0x03) << 6;
		output0 |= ((input0 >> 8) & 0x03) << 8;
		output0 |= (input0 & 0x03) << 10;
		output0 |= ((input1 >> 8) & 0x03) << 12;
		output0 |= (input1 & 0x03) << 14;

		output1 = (input0 >> 14) & 0x03;
		output1 |= ((input0 >> 6) & 0x03) << 2;
		output1 |= ((input1 >> 14) & 0x03) << 4;
		output1 |= ((input1 >> 6) & 0x03) << 6;
		output1 |= ((input0 >> 12) & 0x03) << 8;
		output1 |= ((input0 >> 4) & 0x03) << 10;
		output1 |= ((input1 >> 12) & 0x03) << 12;
		output1 |= ((input1 >> 4) & 0x03) << 14;

		output[i] = (uint8_t)(output0 >> 8);
		output[i+1] = (uint8_t)output0;
		output[i+2] = (uint8_t)(output1 >> 8);
		output[i+3] = (uint8_t)output1;
	}
}