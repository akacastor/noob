// Saleae Logic8 interface to QPSK-demodulated OOB transport stream
// Chris Gerlinsky, 2016
//
// this is based on the demo application for the SaleaeDeviceSdk-1.1.9

#include <SaleaeDeviceApi.h>

#include <memory>
#include <iostream>
#include <string>


void __stdcall OnConnect( U64 device_id, GenericInterface* device_interface, void* user_data );
void __stdcall OnDisconnect( U64 device_id, void* user_data );
void __stdcall OnReadData( U64 device_id, U8* data, U32 data_length, void* user_data );
void __stdcall OnWriteData( U64 device_id, U8* data, U32 data_length, void* user_data );
void __stdcall OnError( U64 device_id, void* user_data );

//#define USE_LOGIC_16 0
// note: ProcessData() is expecting U8* data (from Logic8)

#if( USE_LOGIC_16 )
	Logic16Interface* gDeviceInterface = NULL;
#else
	LogicInterface* gDeviceInterface = NULL;
#endif

U64 gLogicId = 0;
U32 gSampleRateHz = 8000000;        // 8 MHz sample rate is enough for clean capture of input data (4 MHz is too slow)


int main( int argc, char *argv[] )
{
	DevicesManagerInterface::RegisterOnConnect( &OnConnect );
	DevicesManagerInterface::RegisterOnDisconnect( &OnDisconnect );
	DevicesManagerInterface::BeginConnect();

	std::cerr << std::uppercase << "Devices are currently set up to read and write at " << gSampleRateHz << " Hz.  You can change this in the code." << std::endl;

    while( true )
    {
        if( gDeviceInterface == NULL )
        {
            sleep(1);
            continue;
        }
    	if( gDeviceInterface->IsStreaming() == false )
            gDeviceInterface->ReadStart();        

        sleep(1);            
	} 


	return 0;
}


void __stdcall OnConnect( U64 device_id, GenericInterface* device_interface, void* user_data )
{    
#if( USE_LOGIC_16 )

	if( dynamic_cast<Logic16Interface*>( device_interface ) != NULL )
	{
		std::cout << "A Logic16 device was connected (id=0x" << std::hex << device_id << std::dec << ")." << std::endl;

		gDeviceInterface = (Logic16Interface*)device_interface;
		gLogicId = device_id;

		gDeviceInterface->RegisterOnReadData( &OnReadData );
		gDeviceInterface->RegisterOnWriteData( &OnWriteData );
		gDeviceInterface->RegisterOnError( &OnError );

		U32 channels[16];
		for( U32 i=0; i<16; i++ )
			channels[i] = i;

		gDeviceInterface->SetActiveChannels( channels, 16 );
		gDeviceInterface->SetSampleRateHz( gSampleRateHz );
	}

#else

	if( dynamic_cast<LogicInterface*>( device_interface ) != NULL )
	{
		std::cerr << "A Logic device was connected (id=0x" << std::hex << device_id << std::dec << ")." << std::endl;

		gDeviceInterface = (LogicInterface*)device_interface;
		gLogicId = device_id;

		gDeviceInterface->RegisterOnReadData( &OnReadData );
		gDeviceInterface->RegisterOnWriteData( &OnWriteData );
		gDeviceInterface->RegisterOnError( &OnError );

		gDeviceInterface->SetSampleRateHz( gSampleRateHz );
	}

#endif
}

void __stdcall OnDisconnect( U64 device_id, void* user_data )
{
	if( device_id == gLogicId )
	{
		std::cerr << "A device was disconnected (id=0x" << std::hex << device_id << std::dec << ")." << std::endl;
		gDeviceInterface = NULL;
	}
}


U8 oob_drx_mask = 0x01;             // OOB DRX signal = Channel 0
U8 oob_crx_mask = 0x02;             // OOB CRX signal = Channel 1


// process data received from the logic analyzer via OnReadData(), analyze the data and convert it to a bitstream
int ProcessData( U8* data, U32 data_length )
{
    static int ts_idx = 0;          // index into ts packet (192 bytes including fec)  ts_idx[0] = 0x47 or 0x64 alternating (due to ts randomization)
                                    // ts_idx range: 0 - 191 (192 = packet complete, wrap to 0)
    static int crx_status = 0;      // current status of CRX signal (data is valid on falling edge of CRX)
    static int input_byte = 0;      // current input byte being received
    static int input_bit_count = 0; // # of bits received in current byte (0-7 - when 8 is reached, wrap to 0)
    static int sync = 0;
    int i;
    
    
    for( i=0; i<data_length; i++ )
    {
        if( (data[i] & oob_crx_mask) && (crx_status==0) )          // rising edge of CRX
        {
            crx_status = 1;
        }
        else if( ((data[i] & oob_crx_mask)==0) && crx_status )     // falling edge of CRX
        {   // data bit on DRX is valid
            crx_status = 0;
            input_byte <<= 1;
            if( data[i] & oob_drx_mask )
                input_byte |= 1;     // received bit is '1'
            input_bit_count++;
            
            if( input_bit_count > 7 )
            {   // a complete byte has been received to input_byte
                if( ts_idx )
                {   // output next byte in block (we are currently synced)
                  	std::cout << std::hex << (char)input_byte;
                	ts_idx++;
                	if( ts_idx>=192 )
                	    ts_idx = 0;
                }
                else
                {   // we are not synced
                    if( input_byte == 0x47 || input_byte == 0x64 )  
                    {   // looks like a valid sync
                    	std::cout << std::hex << (char)input_byte;
                    	ts_idx=1;
                    	
                        if( !sync )
                        {   // we were syncd, but lost it
                            sync = 1;
                            std::cerr << "Sync OK." << std::endl;
                        }
                    }
                    else
                    {   // currently not synced, try losing a bit
                        input_byte &= 0x7F;
                        input_bit_count=7;
                        if( sync )
                        {   // we were syncd, but lost it
                            sync = 0;
                            std::cerr << "Resync";
                        }
                        std::cerr << "!";
                        continue;
                    }
                }
        
                input_bit_count = 0;    // setup to receive next input byte
                input_byte = 0;         // setup to receive next input byte
            }
        }
    }
    
    
    return 0;
}


void __stdcall OnReadData( U64 device_id, U8* data, U32 data_length, void* user_data )
{
#if( USE_LOGIC_16 )
//	std::cout << "Read " << data_length/2 << " words, starting with 0x" << std::hex << *(U16*)data << std::dec << std::endl;
#else
//	std::cout << "Read " << data_length << " bytes, starting with 0x" << std::hex << (int)*data << std::dec << std::endl;
#endif

// process data received from the logic analyzer via OnReadData(), analyze the data and convert it to a bitstream
    ProcessData( data, data_length );

	//you own this data.  You don't have to delete it immediately, you could keep it and process it later, for example, or pass it to another thread for processing.
	DevicesManagerInterface::DeleteU8ArrayPtr( data );
}




void __stdcall OnWriteData( U64 device_id, U8* data, U32 data_length, void* user_data )
{
#if( USE_LOGIC_16 )

#else
	static U8 dat = 0;

	//it's our job to feed data to Logic whenever this function gets called.  Here we're just counting.
	//Note that you probably won't be able to get Logic to write data at faster than 4MHz (on Windows) do to some driver limitations.

	//here we're just filling the data with a 0-255 pattern.
	for( U32 i=0; i<data_length; i++ )
	{
		*data = dat;
		dat++;
		data++;
	}

//	std::cout << "Wrote " << data_length << " bytes of data." << std::endl;
#endif
}

void __stdcall OnError( U64 device_id, void* user_data )
{
    std::cerr << "A device reported an Error.  This probably means that it could not keep up at the given data rate, or was disconnected. The capture will re-start automatically, you will have gaps in the data." << std::endl;
	//note that you should not attempt to restart data collection from this function -- you'll need to do it from your main thread (or at least not the one that just called this function).
}