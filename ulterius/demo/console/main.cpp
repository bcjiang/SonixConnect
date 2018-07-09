#include "stdafx.h"

#include <fstream>
using std::ofstream;
#include <iostream>
using std::cout;
using std::endl;
#define EXIT_FN    0
#define BUFFERSIZE (4 * 1024 * 1024)

bool newFrame(void* data, int type, int sz, bool cine, int frmnum);
bool paramCallback(void* paramID, int ptX, int ptY);
char* getWord(char* in, char sep, char* out, int sz);

char gBuffer[BUFFERSIZE];

void main()
{
    ulterius ult;
    int i, v, b, sel = -1;
    char str[80], list[1000], * pStr;

    uParam param;
    uDataDesc desc;

    ult.setCallback(newFrame);
    ult.setParamCallback(paramCallback);

    while (sel != EXIT_FN)
    {
        printf("<<<Ulterius Client Demo>>>\n");
        printf("\n");
        printf("Make a selection:\n");
        printf("\n");
        printf("(1)  Connect\n");
        printf("(2)  Disconnect\n");
        printf("\n");
        printf("(3)  Get Active Mode\n");
        printf("(4)  Get Active Probe\n");
        printf("(5)  Get Active Preset\n");
        printf("(6)  Get Freeze State\n");
        printf("(7)  List Connected Probes\n");
        printf("(8)  List Presets\n");
        printf("(9)  List Patient Fields\n");
        printf("(10)  Select Mode\n");
        printf("(11) Select Probe\n");
        printf("(12) Select Preset\n");
        printf("(13) Freeze\n");
        printf("\n");
        printf("(14) List Parameters\n");
        printf("(15) Get Parameter Value\n");
        printf("(16) Set Parameter Value\n");
        printf("(17) Inc Parameter\n");
        printf("(18) Dec Parameter\n");
        printf("\n");
        printf("(19) Set Acquisition Flags\n");
        printf("(20) Get Data Availability\n");
        printf("(21) Get Data Descriptor\n");
        printf("(22) Get Frame Count from Cine\n");
        printf("(23) Get Data from Cine\n");
        printf("(24) Get Compression\n");
        printf("(25) Set Compression\n");
        printf("\n");
        printf("(26) Set Inject Mode\n");
        printf("(27) Get Inject Mode\n");
        printf("(28) Inject Image\n");
        printf("\n");
        printf("(%d) Exit\n\n", EXIT_FN);

        scanf("%d", &sel);

        system("cls");

        // connect to a server
        if (sel == 1)
        {
			printf("Press enter to connect. "); //10.162.34.191 for B08A machine
            //scanf("%s", str);
            if (!ult.connect("10.162.34.191"))
            {
                printf("Error: Could not connect\n");
            }
            else
            {
                printf("Successfully connected\n");
            }
        }
        // disconnect from the server
        else if (sel == 2)
        {
            ult.disconnect();
        }
        // retreive active mode
        else if (sel == 3)
        {
            i = ult.getActiveImagingMode();
            if (i != -1)
            {
                printf("\nActive Imaging Mode: %d", i); // Get 0 for startup
            }
        }
        // retreive active probe
        else if (sel == 4)
        {
            if (ult.getActiveProbe(str, 80))
            {
                printf("\nActive Probe: %s\n", str);
            }
        }
        // retreive the current preset
        else if (sel == 5)
        {
            if (ult.getActivePreset(str, 80))
            {
                printf("\nActive Preset: %s", str); //Get ABD-General (4DC7-3 40mm)
            }
        }
        // retreive the freeze state
        else if (sel == 6)
        {
            i = ult.getFreezeState();
            if (i != -1)
            {
                printf("\nFreeze State: %s", i ? "FROZEN" : "IMAGING"); //Get "IMAGING" when start up
            }
        }
        // list all the connected probes
        else if (sel == 7)
        {
            if (ult.getProbes(list, 1000))
            {
                i = 1;
                pStr = list;
                printf("\nConnected Probes List:\n");
                while ((pStr = getWord(pStr, '\n', str, 80)) != 0)
                {
                    printf("(%d) %s\n", i++, str); //If not connected, will show as 4DC7-3/40
                }
            }
        }
        // list all the presets for the active probe
        else if (sel == 8)
        {
            if (ult.getPresets(list, 1000))
            {
                i = 1;
                pStr = list;
                printf("\nPresets List:\n");
                while ((pStr = getWord(pStr, '\n', str, 80)) != 0)
                {
                    printf("(%d) %s\n", i++, str);
                }
            }
        }
        // list the patient fields for the current study
        else if (sel == 9)
        {
            if (ult.getPatientInfo(list, 1000))
            {
                i = 1;
                pStr = list;
                printf("\nPatient Fields:\n");
                while ((pStr = getWord(pStr, '\n', str, 80)) != 0)
                {
                    printf("(%d) %s\n", i++, str);
                }
            }
        }
        // select imaging mode
        else if (sel == 10)
        {
            printf("\nSelect mode: ");
            scanf("%d", &i);
            ult.selectMode(i);
        }
        // select probe connector
        else if (sel == 11)
        {
            printf("\nSelect probe connector (0, 1, 2): ");
            scanf("%d", &i);
            ult.selectProbe(i);
        }
        // activate a preset
        else if (sel == 12)
        {
            printf("\nEnter Preset Name: ");
            scanf("%s", str);
            ult.selectPreset(str);
        }
        // toggle freeze
        else if (sel == 13)
        {
            ult.toggleFreeze();
        }
        // list of all the parameters
        else if (sel == 14)
        {
            param;
            i = 0;
            printf("\nParameters List:\n");

            while (ult.getParam(i++, param))
            {
                printf("(%s) %s\n", param.id, param.name);
            }
        }
        // retreive a parameter value
        else if (sel == 15)
        {
            printf("\nEnter Parameter ID: ");
            fflush(stdin);
            gets(str);

            if (ult.getParamValue(str, v))
            {
                printf("Value = %d\n", v);
            }
        }
        // set a parameter value
        else if (sel == 16)
        {
            printf("\nEnter Parameter ID: ");
            fflush(stdin);
            gets(str);
            fflush(stdin);
            printf("\nEnter Value: ");
            scanf("%d", &v);

            ult.setParamValue(str, v);
        }
        // increment a parameter
        else if (sel == 17)
        {
            printf("\nEnter Parameter ID: ");
            fflush(stdin);
            gets(str);
            ult.incParam(str);
        }
        // decrement a parameter
        else if (sel == 18)
        {
            printf("\nEnter Parameter ID: ");
            fflush(stdin);
            gets(str);
            ult.decParam(str);
        }
        // set the data types to acquire in real-time or through the cine
        else if (sel == 19)
        {
            printf("\nEnter Mask in Hex (00000000): ");
            scanf("%x", &i);
            ult.setDataToAcquire(i);
        }
        // get the availability of a data type given the current imaging mode
        else if (sel == 20)
        {
            printf("\nEnter Data Type: ");
            scanf("%d", &i);
            if (ult.isDataAvailable((uData)i))
            {
                printf("Data is available");
            }
            else
            {
                printf("Data is not available");
            }
        }
        // retreive a data descriptor
        else if (sel == 21)
        {
            printf("\nEnter Data Type: ");
            scanf("%d", &i);
            if (ult.getDataDescriptor((uData)i, desc))
            {
                printf("Width: %d\nHeight: %d\nSample Size: %d\n", desc.w, desc.h, desc.ss);
            }
        }
        // retreive the frame count of a certain data type (system must be frozen)
        else if (sel == 22)
        {
            printf("\nEnter Data Type: ");
            scanf("%d", &i);
            printf("Frame Count: %d\n", ult.getCineDataCount((uData)i));
        }
        // get a frame from the cine (system must be frozen)
        else if (sel == 23)
        {
            printf("\nEnter Data Type: ");
            scanf("%d", &i);
            printf("\nEnter Frame #: ");
            scanf("%d", &v);
            printf("\nEnter Retreival Mode (0 = blocking / 1 = callback): ");
            scanf("%d", &b);
            ult.getCineData((uData)i, v, b ? true : false, (void**)&gBuffer, BUFFERSIZE);
        }
        // retreive compression status
        else if (sel == 24)
        {
            i = ult.getCompressionStatus();
            if (i != -1)
            {
                printf("\nCompression Status: %s", i ? "ON" : "OFF");
            }
        }
        // set compression on or off
        else if (sel == 25)
        {
            printf("\nSet compression (0 = off / 1 = on): ");
            scanf("%d", &i);
            ult.setCompressionStatus(i);
        }
        // set the inject mode
        else if (sel == 26)
        {
            printf("\nEnter Inject Mode (0 = off / 1 = on): ");
            scanf("%d", &i);
            ult.setInjectMode(i ? true : false);
        }
        // retrieve the inject mode
        else if (sel == 27)
        {
            printf("\nInject Mode = %d", ult.getInjectMode() ? true : false);
        }
        // inject an image
        else if (sel == 28)
        {
            // generate some random data
            for (i = 0; i < BUFFERSIZE; i++)
            {
                gBuffer[i] = (char)(rand() % 255);
            }

            // inject a random image
            if (ult.injectImage(gBuffer, 640, 480, 32, false))
            {
                printf("\nInject Image Passed");
            }
            else
            {
                printf("\nInject Image Failed");
            }
        }

        if (sel == EXIT_FN)
        {
            break;
        }

        printf("\n\n\nPress any key to continue...\n");
        while (!_kbhit())
        {
        }
        system("cls");
    }

    if (ult.isConnected())
    {
        ult.disconnect();
    }
}

// the callback function used when there is a new frame of data received
bool newFrame(void* data, int type, int sz, bool cine, int frmnum)
{
    if (!data || !sz)
    {
        printf("Error: no actual frame data received\n");
        return false;
    }

    if (BUFFERSIZE < sz)
    {
        printf("Error: frame too large for current buffer\n");
        return false;
    }

    printf("[Rx] type:(%d) size:(%d) cine:(%d) tag:(%d)\n", type, sz, cine, frmnum);

    // make sure we dont do an operation that takes longer than the acquisition frame rate
    memcpy(gBuffer, data, sz);
	unsigned char* chardata = reinterpret_cast<unsigned char*>(data);
	cout<<"test:"<<endl;
	cout<<chardata[0]<<" ; "<<chardata[1]<<endl;
	//// Write buffer to file
	//ofstream outbuffer;
	//outbuffer.open("G:\\buffer.txt");
	//if(!outbuffer){
	//	cout<<"Opening file failed"<<endl;
	//}
	//outbuffer<<gBuffer<<endl;
	//if(!outbuffer){
	//	cout<<"Opening file failed"<<endl;
	//}

    return true;
}

bool paramCallback(void* /*paramID*/, int /*ptX*/, int /*ptY*/)
{
    return true;
}

// used to parse words from retreived lists
char* getWord(char* in, char sep, char* out, int sz)
{
    int pos = 0;

    strcpy(out, "");

    while (in[pos] != sep)
    {
        if (in[pos] == '\0')
        {
            return 0;
        }

        out[pos] = in[pos];

        if (++pos >= sz)
        {
            return 0;
        }
    }

    out[pos] = '\0';

    return in + (pos + 1);
}
