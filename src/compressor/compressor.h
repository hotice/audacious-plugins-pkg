/*
 * Dynamic Range Compression Plugin for Audacious
 * Copyright 2010 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

extern float compressor_target, compressor_range;

void compressor_config_load (void);
void compressor_config_save (void);

void compressor_init (void);
void compressor_cleanup (void);
void compressor_start (int * channels, int * rate);
void compressor_process (float * * data, int * samples);
void compressor_flush (void);
void compressor_finish (float * * data, int * samples);
int compressor_decoder_to_output_time (int time);
int compressor_output_to_decoder_time (int time);
