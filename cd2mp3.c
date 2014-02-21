/* This code was made by Samuel Audet <guardia@cam.org>.  Use it, but give
   me credits, thanks  */

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>


/* reverse channels routine */

struct
{
	char R1;
	char I2;
	char F3;
	char F4;
	unsigned long WAVElen;
	struct
	{
		char W1;
		char A2;
		char V3;
		char E4;
		char f1;
		char m2;
		char t3;
		char space;
		unsigned long fmtlen;
		struct
		{
			unsigned short FormatTag;
			unsigned short Channels;
			unsigned long SamplesPerSec;
			unsigned long AvgBytesPerSec;
			unsigned short BlockAlign;
			unsigned short BitsPerSample; /* format specific for PCM */
		} fmt;
		struct
		{
			char d1;
			char a2;
			char t3;
			char a4;
			unsigned long datalen;
			/* from here you insert your PCM data */
		} data;
	} WAVE;
} RIFF;

/* returns 1 if found a good wav file, 0 if not */

int readRIFF(int hfile, int *revepos, int *datapos)
{
	*revepos = -1;
	*datapos = -1;

   lseek(hfile, 0, SEEK_SET);
   read(hfile, &RIFF.R1,4);
	if(RIFF.R1 == 'R' && RIFF.I2 == 'I' && RIFF.F3 == 'F' && RIFF.F4 == 'F')
	{
      read(hfile, &RIFF.WAVElen,4);
      read(hfile, &RIFF.WAVE.W1,4);
		if(RIFF.WAVE.W1 == 'W' && RIFF.WAVE.A2 == 'A' && RIFF.WAVE.V3 == 'V' && RIFF.WAVE.E4 == 'E')
		{
			int foundfmt = 0;
			do
			{
            if(!read(hfile, &RIFF.WAVE.f1,4)) break;
            if(!read(hfile, &RIFF.WAVE.fmtlen,4)) break;
				if(RIFF.WAVE.f1 == 'f' && RIFF.WAVE.m2 == 'm' && RIFF.WAVE.t3 == 't' &&
					RIFF.WAVE.fmtlen == sizeof(RIFF.WAVE.fmt))
				{
               if(!read(hfile, &RIFF.WAVE.fmt,sizeof(RIFF.WAVE.fmt))) break;
               foundfmt = 1;
				}
				else
				{
					/* we skip things we don't know */
               if(lseek(hfile,RIFF.WAVE.fmtlen,SEEK_CUR)) break;
				}
			}
			while(!foundfmt);

			if(foundfmt)
			{
				char buffer[4];
				unsigned long length;
				/* if this is not a standard PCM sample, abort */
				if(RIFF.WAVE.fmt.FormatTag != 1) return 0;

				do
				{
               if(!read(hfile, &buffer,4)) break;
               if(!read(hfile, &length,4)) break;

					/* check for the presence of our tag */
					if(buffer[0] == 'r' && buffer[1] == 'e' && buffer[2] == 'v' && buffer[3] == 'e')
					{
                  int garbage;

						if(RIFF.WAVE.fmt.BitsPerSample == 16)
						{
                     if(length == 2) *revepos = _tell(hfile)-8;
						}
						else if(RIFF.WAVE.fmt.BitsPerSample == 8)
						{
                     if(length == 1) *revepos = _tell(hfile)-8;
						}
                  read(hfile,&garbage,length);
					}
               else if(buffer[0] == 'd' && buffer[1] == 'a' && buffer[2] == 't' && buffer[3] == 'a')
					{
						RIFF.WAVE.data.d1 = 'd';
						RIFF.WAVE.data.a2 = 'a';
						RIFF.WAVE.data.t3 = 't';
						RIFF.WAVE.data.a4 = 'a';
						RIFF.WAVE.data.datalen = length;
                  *datapos = _tell(hfile)-8;
						return 1; /* tada */
					}
					else
						/* we skip things we don't know */
                  if(lseek(hfile,length,SEEK_CUR)) break;
				}
				while(1);
			}
		}
	}
	return 0;
}

int reversewav(char *filename)
{
	int revepos, datapos;
   int hfile;
	unsigned long length;
	unsigned long null = 0;

   hfile = open(filename, O_RDWR | O_BINARY, 0);
   if(!hfile)
	{
      perror("open()");
		return 3;
	}

   if(!readRIFF(hfile,&revepos,&datapos))
	{
		printf("The file could not be identified as a standard RIFF WAVE PCM stream.\n");
		return 2;
	}

	if(revepos < 0)
	{
      if(RIFF.WAVE.fmt.BitsPerSample == 16)
			length = 2;
		else if(RIFF.WAVE.fmt.BitsPerSample == 8)
			length = 1;

      RIFF.WAVE.data.datalen -= 4+4+length;

      lseek(hfile,datapos,SEEK_SET);
      write(hfile, "reve",4);
      write(hfile, &length, 4);
      write(hfile, &null,length); /* garbage */
      write(hfile, &RIFF.WAVE.data,sizeof(RIFF.WAVE.data));
	}
	else
	{
      if(RIFF.WAVE.fmt.BitsPerSample == 16)
			length = 2;
		else if(RIFF.WAVE.fmt.BitsPerSample == 8)
			length = 1;

      RIFF.WAVE.data.datalen += 4+4+length;

      lseek(hfile,revepos,SEEK_SET);
      write(hfile, &RIFF.WAVE.data,sizeof(RIFF.WAVE.data));
      write(hfile, &null,4); /* ex-"reve" */
      write(hfile, &null,4); /* ex-length of reve chunk */
      write(hfile, &null,length);
	}

   close(hfile);
	return 0;
}

void help(char *program)
{
   printf("\nCD2MP3 1.10 (C) 1998 Samuel Audet <guardia@cam.org>\n"
          "\n"
          "%s <x[:]> <\"T[rack]\"> <#to#|#,#,...> [basename] [/L[eech] <Leech options>] "
          "[/T[oMPG] <ToMPG options>] [/M[P3Enc] <MP3Enc options>] [/U[T[oMPG]|M[P3Enc]] [/I]"
          "[/DW[AV]|M[P3] <directory>]\n"
          "\n"
          "   x:        CD Drive letter \n"
          "   \"T\"       mandatory command \n"
          "   #to#      from track # to track # \n"
          "   #,#,...   track enumaration \n"
          "   basename  default \"Track\" \n"
          "   /L        default none \n"
          "   /T        default \"-B64 -M0\" \n"
          "   /M        default \"-v -qual 0\" \n"
          "   /U        Use ToMPG or MP3Enc \n"
          "   /I        Do not run encoder in idle priority \n"
          "   /D        Temporary dir for the WAV and permanent dir for MP3, default current \n"
          "   /R        Reverse channels of the WAV grabbed from the CD \n"
          "\n"
          "Example: [C:\\] %s e: track 1to5 Madonna /L \"-d5 -j20\" /T \"-B64 -M0\" /DW d:\\temp /DM e:\\mp3 \n ",
                                        program,program);
}

int main(int argc, char *argv[])
{
   enum WHICHENCODER { tompg, mp3enc } whichencoder = tompg;
   int i, e; /* indexes */
   char *cddrive, *command, lowprio, reversechannels,
        *leechopt = "", *tompgopt = "-B64 -M0", *mp3encopt = "-v -qual 0",
        wavdir[CCHMAXPATH] = {0}, mp3dir[CCHMAXPATH] = {0};
   char *basename = "Track";
   int tracklist[200] = {0};

   if(argc<3)
   {
      help(argv[0]);
      return 1;
   }
   i = 1;
   cddrive = argv[i++];
   command = argv[i++];

   switch(toupper(*argv[2]))
   {
      case 'T':
         if(argc > i)
         {
            int totrack = 0;
            char *next = argv[i], *temp;

            temp = strstr(strupr(argv[i]),"TO");
            if(temp) totrack = atoi(temp+2);

            e = 0;
            tracklist[e] = atoi(argv[i]);
            while(tracklist[e++] < totrack)
               tracklist[e] = tracklist[e-1]+1;
            while(next = strchr(next,','))
               tracklist[e] = atoi(++next);

            i++;
            break;
         }
   }

   lowprio = TRUE;
   reversechannels = FALSE;
   for(  ; i < argc; i++)
   {
      if((argv[i][0] == '/') || (argv[i][0] == '-'))
         switch(toupper(argv[i][1]))
         {
            case 'L': leechopt = argv[++i]; break;
            case 'T': tompgopt = argv[++i]; break;
            case 'M': mp3encopt = argv[++i]; break;
            case 'U':
              if(toupper(argv[i][2]) == 'M')
                 whichencoder = mp3enc;
              else if(toupper(argv[i][2]) == 'T')
                 whichencoder = tompg;
              break;
            case 'I': lowprio = FALSE; break;
            case 'R': reversechannels = TRUE; break;
            case 'D':
              if(toupper(argv[i][2]) == 'W')
              {
                 strcpy(wavdir,argv[++i]);
                 if(*(strchr(wavdir,'\0')-1) != '\\')
                    strcat(wavdir,"\\");
              }
              else if(toupper(argv[i][2]) == 'M')
              {
                 strcpy(mp3dir,argv[++i]);
                 if(*(strchr(mp3dir,'\0')-1) != '\\')
                    strcat(mp3dir,"\\");
              }
              break;
         }
      else
         basename = argv[i];
   }

   e = -1;
   while(tracklist[++e])
   {
      STARTDATA startdata;
      ULONG idSession;
      PID childpid;
      SWBLOCK *switchlist = NULL;
      HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);
      ULONG count,buffersize;
      RESULTCODES childrc;
      BOOL finished;
      APIRET rc;
      char buildcmdline[512],mp3file[256],wavfile[256];
      char *program,*temp;

      sprintf(wavfile,"%s%s_%.2d.wav",wavdir,basename,tracklist[e]);
      sprintf(mp3file,"%s%s_%.2d.mp3",mp3dir,basename,tracklist[e]);

      printf("Grabbing Track #%d... ",tracklist[e]);
      fflush(stdout);

      /* Leech part */
      program = "leech.exe";
      printf("Launching Leech... ");
      fflush(stdout);

      strcpy(buildcmdline,program);
      temp = strchr(buildcmdline,'\0')+1;
      sprintf(temp,"%s track %d \"%s%s\" %s",cddrive,tracklist[e],wavdir,basename,leechopt);
      *(strchr(temp,'\0')+1) = '\0';

      rc = DosExecPgm(NULL,0,EXEC_SYNC,buildcmdline,NULL,&childrc,program);
      if(rc != NO_ERROR)
      {
         fprintf(stderr,"Error: could not start %s.\n",program);
         return 3;
      }
      if( childrc.codeTerminate > 0  ||
          childrc.codeResult   == 99    ) /* Leech intercepts break?? */
      {
         fprintf(stderr,"\rAborting, %s killed on purpose.\n",program);
         return 2;
      }

      if(childrc.codeResult != 0)
      {
         printf("Error: Leech returned with errorlevel %d, aborting MP3 encoding.",childrc.codeResult);
         fflush(stdout);
         remove(wavfile);
         continue;
      }

      /* reverse channels part */
      if(reversechannels)
      {
         printf("Reversing channels of the WAV file...");
         if(reversewav(wavfile))
           printf("WAV channel reversing didn't work...");
      }

      /* ToMPG and MP3ENC part */

      switch(whichencoder)
      {
         case tompg:
            program = "tompg.exe";
            printf("Launching ToMPG... ");
            fflush(stdout);

            sprintf(buildcmdline,"\"%s\" \"%s\" %s",wavfile, mp3file, tompgopt);
            break;

         case mp3enc:
            program = "mp3enc.exe";
            printf("Launching MP3Enc... ");
            fflush(stdout);

            sprintf(buildcmdline,"-if \"%s\" -of \"%s\" %s",wavfile, mp3file, mp3encopt);
            break;
      }

      if(lowprio)
         DosSetPriority(PRTYS_PROCESS, PRTYC_IDLETIME, 0, 0);

      memset(&startdata,0,sizeof(startdata));
      startdata.Length = sizeof(startdata);
   /*   startdata.Related = SSF_RELATED_CHILD; */
      startdata.SessionType = SSF_TYPE_PM;
      startdata.PgmName = program;
      startdata.PgmInputs = buildcmdline;
      startdata.Environment = "NOWIN32LOG=anything\0";
      startdata.InheritOpt = 1;

      rc = DosStartSession(&startdata,&idSession,&childpid);
      if(rc != NO_ERROR && rc != ERROR_SMG_START_IN_BACKGROUND)
      {
         fprintf(stderr,"Error: could not start %s.\n", program);
         return 3;
      }

      if(lowprio)
         DosSetPriority(PRTYS_PROCESS, PRTYC_REGULAR, 0, 0);

      finished = FALSE;
      do
      {
         DosSleep(500);

         count = WinQuerySwitchList(hab,NULL,0);
         buffersize = count * sizeof(SWENTRY) + sizeof(ULONG);
         if(_msize(switchlist) < buffersize)
         {
            free(switchlist);
            switchlist = (PSWBLOCK) malloc(2*buffersize);
         }
         if (switchlist && count)
         {
            count = WinQuerySwitchList(hab,switchlist,buffersize);

            /* go into all switch list items */
            for (i = 0 ; i < count ; i++)
            {
               if ( strstr(switchlist->aswentry[i].swctl.szSwtitle, program) &&
                    strstr(switchlist->aswentry[i].swctl.szSwtitle, "Completed:") &&
                    strstr(switchlist->aswentry[i].swctl.szSwtitle, wavfile))
               {
                  DosKillProcess(DKP_PROCESS,switchlist->aswentry[i].swctl.idProcess);
                  finished = TRUE;
                  break;
               }
            }
         }
      } while(!finished);

      printf("Done extracting \"%s\"! ... ", mp3file);
      printf("Deleting \"%s\".\n",wavfile);
      fflush(stdout);
      remove(wavfile);
   }

   return 0;
}
