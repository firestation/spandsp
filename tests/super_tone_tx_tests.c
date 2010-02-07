/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_generate_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: super_tone_tx_tests.c,v 1.4 2005/09/01 17:06:45 steveu Exp $
 */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <math.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

#define OUT_FILE_NAME   "super_tone.wav"

AFfilehandle outhandle;
AFfilesetup filesetup;

super_tone_tx_step_t *tone_tree = NULL;

static void play_tones(super_tone_tx_state_t *tone, int max_samples)
{
    int16_t amp[8000];
    int len;
    int outframes;
    int i;
    int total_length;

    i = 500;
    total_length = 0;
    do
    {
        len = super_tone_tx(tone, amp, 160);
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing wave file\n");
            exit(2);
        }
        total_length += len;
    }
    while (len > 0  &&  --i > 0);
    printf("Tone length = %d samples (%dms)\n", total_length, total_length/8);
}
/*- End of function --------------------------------------------------------*/

static int parse_tone(super_tone_tx_step_t **tree, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    xmlChar *x;
    float f1;
    float f2;
    float f_tol;
    float l1;
    float l2;
    float length;
    float length_tol;
    int cycles;
    super_tone_tx_step_t *treep;

    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "step") == 0)
        {
            printf("Step - ");
            /* Set some defaults */
            f1 = 0.0;
            f2 = 0.0;
            f_tol = 1.0;
            l1 = -11.0;
            l2 = -11.0;
            length = 0.0;
            length_tol = 10.0;
            cycles = 1;
            if ((x = xmlGetProp(cur, (const xmlChar *) "freq")))
            {
                sscanf((char *) x, "%f [%f%%]", &f1, &f_tol);
                sscanf((char *) x, "%f+%f [%f%%]", &f1, &f2, &f_tol);
                printf("Frequency=%.2f+%.2f [%.2f%%]", f1, f2, f_tol);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "level")))
            {
                if (sscanf((char *) x, "%f+%f", &l1, &l2) < 2)
                    l2 = l1;
                printf("Level=%.2f+%.2f", l1, l2);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "length")))
            {
                sscanf((char *) x, "%f [%f%%]", &length, &length_tol);
                printf("Length=%.2f [%.2f%%]", length, length_tol);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recognition-length")))
                printf("Recognition length='%s'", x);
            if ((x = xmlGetProp(cur, (const xmlChar *) "cycles")))
            {
                if (strcasecmp((char *) x, "endless") == 0)
                    cycles = 0;
                else
                    cycles = atoi((char *) x);
                printf("Cycles=%d ", cycles);
            }
            if ((x = xmlGetProp(cur, (const xmlChar *) "recorded-announcement")))
                printf("Recorded announcement='%s'", x);
            printf("\n");
            treep = super_tone_tx_make_step(NULL,
                                            f1,
                                            l1,
                                            f2,
                                            l2,
                                            length*1000.0 + 0.5,
                                            cycles);
            *tree = treep;
            tree = &(treep->next);
            parse_tone(&(treep->nest), doc, ns, cur);
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return  0;
}
/*- End of function --------------------------------------------------------*/

static void parse_tone_set(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    super_tone_tx_state_t tone;

    printf("Parsing tone set\n");
    cur = cur->xmlChildrenNode;
    while (cur)
    {
        if (strcmp((char *) cur->name + strlen((char *) cur->name) - 5, "-tone") == 0)
        {
            printf("Hit %s\n", cur->name);
            tone_tree = NULL;
            parse_tone(&tone_tree, doc, ns, cur);
            super_tone_tx_init(&tone, tone_tree);
printf("Len %p %p %d %d\n", tone.levels[0], tone_tree, tone_tree->length, tone_tree->tone);
            play_tones(&tone, 99999999);
            super_tone_tx_free(tone_tree);
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

static void get_tone_set(char *tone_file, char *set_id)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
#if 0
    xmlValidCtxt valid;
#endif
    xmlChar *x;
    
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();
    doc = xmlParseFile(tone_file);
    if (doc == NULL)
    {
        fprintf(stderr, "No document\n");
        exit(2);
    }
    /*endif*/
    xmlXIncludeProcess(doc);
#if 0
    if (!xmlValidateDocument(&valid, doc))
    {
        fprintf(stderr, "Invalid document\n");
        exit(2);
    }
    /*endif*/
#endif
    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(doc)) == NULL)
    {
        fprintf(stderr, "Empty document\n");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "global-tones"))
    {
        fprintf(stderr, "Document of the wrong type, root node != global-tones");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode(cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
        exit(2);
    /*endif*/
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "tone-set") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "uncode")))
            {
                if (strcmp((char *) x, set_id) == 0)
                    parse_tone_set(doc, ns, cur);
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    xmlFreeDoc(doc);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    filesetup = afNewFileSetup ();
    if (filesetup == AF_NULL_FILESETUP)
    {
    	fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, 8000.0);
    //afInitCompression(filesetup, AF_DEFAULT_TRACK, AF_COMPRESSION_G711_ALAW);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    get_tone_set("../spandsp/global-tones.xml", (argc > 1)  ?  argv[1]  :  "hk");
    if (afCloseFile (outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
