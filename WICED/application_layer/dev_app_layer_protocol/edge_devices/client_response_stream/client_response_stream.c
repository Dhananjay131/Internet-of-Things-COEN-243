//------------------------------------------------------------
// SCU's Internet of Things Research Lab (SIOTLAB)
// Santa Clara University (SCU)
// Santa Clara, California
//------------------------------------------------------------

// This application is based on the Cypress WICED platform

//------------------------------------------------------------

// Client to send information to a server using a custom application protocol
//
// The message sent and the response from the server are echoed to a UART terminal.
//
// This version uses TCP Stream APIs instead of TCP Socket APIs which simplifies the firmware.


#include "wiced.h"
#include "register_map.h"

#define TCP_CLIENT_STACK_SIZE 	(6200)
#define SERVER_PORT 			(6999)


static wiced_ip_address_t serverAddress; //Server address
static wiced_semaphore_t button0_semaphore, button1_semaphore;
static wiced_thread_t buttonUpdate, buttonInquiry;
static wiced_mac_t myMac;


// This function is called by the RTOS when the button is pressed
// It just unlocks the button thread semaphore
void button_isr1(void *arg)
{
    wiced_rtos_set_semaphore(&button1_semaphore);
}

void button_isr0(void *arg)
{
    wiced_rtos_set_semaphore(&button0_semaphore);
}

// sendData:
// This function opens a socket connection to the server
// then sends the state of the LED and gets the response
// The input data is 0=Off, 1=On
// This function also enable inquiring the server for the value of a register

void sendData(char type, uint8_t regAdd, int regVal)
{
	wiced_tcp_socket_t socket;						// The TCP socket
	wiced_tcp_stream_t stream;						// The TCP stream
	char sendMessage[28];
	wiced_result_t result, conStatus;

    // format the data
	if (type == 'W')
	sprintf(sendMessage,"W-BDSC-%02X%02X%02X%02X%02X%02X-%02X-%04X\n",
	        myMac.octet[0], myMac.octet[1], myMac.octet[2],
	        myMac.octet[3], myMac.octet[4], myMac.octet[5],
	        regAdd, regVal);
	else if (type == 'R')
	    sprintf(sendMessage,"R-BDSC-%02X%02X%02X%02X%02X%02X-%02X\n",
	            myMac.octet[0], myMac.octet[1], myMac.octet[2],
	            myMac.octet[3], myMac.octet[4], myMac.octet[5],
	            regAdd);
	else{
	    WPRINT_APP_INFO(("Invalid command type\n")); // echo the message so that the user can see something
	    return;
	}

	WPRINT_APP_INFO(("Prepared Message = %s\n",sendMessage)); // echo the message so that the user can see something

	// Open the connection to the remote server via a socket
	wiced_tcp_create_socket(&socket, WICED_STA_INTERFACE);
	wiced_tcp_bind(&socket,WICED_ANY_PORT);
	conStatus = wiced_tcp_connect(&socket,&serverAddress,SERVER_PORT,2000); // 2 second timeout

    if(conStatus == WICED_SUCCESS)
        WPRINT_APP_INFO(("Successful connection!\n"));
    else
    {
        WPRINT_APP_INFO(("Failed connection!\n"));
        wiced_tcp_delete_socket(&socket);
        return;
    }

	// Initialize the TCP stream
	wiced_tcp_stream_init(&stream, &socket);

	// Send the data via the stream
	wiced_tcp_stream_write(&stream, sendMessage, strlen(sendMessage));
	// Force the data to be sent right away even if the packet isn't full yet
	wiced_tcp_stream_flush(&stream);

    // Get the response back from the server
	char rbuffer[30] = {0}; // The first 27 bytes of the buffer will be sent by the server. Byte 28 will stay 0 to null terminate the string
	uint32_t read_count;
	//result = wiced_tcp_stream_read(&stream, rbuffer, 27, 500); // Read bytes from the buffer - wait up to 500ms for a response
	result = wiced_tcp_stream_read_with_count( &stream, rbuffer, 28, 500, &read_count );
	if(result == WICED_SUCCESS)
	    WPRINT_APP_INFO(("Server Response = %s\n\n\n",rbuffer));
	else
	    WPRINT_APP_INFO(("Malformed response = %s\n\n\n",rbuffer));

	// Delete the stream and socket
	wiced_tcp_stream_deinit(&stream);
    wiced_tcp_delete_socket(&socket);
}

// buttonThreadMain:
// This function is the thread that waits for button presses and then sends the
// data via the sendData function
//
// This is done as a separate thread to make the code easier to copy to a later program.
void buttonUpdateMain()
{
    // Main Loop: wait for semaphore.. then send the data
	while(1)
	{
		wiced_rtos_get_semaphore(&button1_semaphore,WICED_WAIT_FOREVER);
	    wiced_gpio_output_low( WICED_SH_LED1 );
	    sendData('W', reg_LED1, 0);
		wiced_rtos_get_semaphore(&button1_semaphore,WICED_WAIT_FOREVER);
		sendData('W', reg_LED1, 1);
		wiced_gpio_output_high( WICED_SH_LED1 );
	}
}

void buttonInquiryMain()
{
    while(1)
    {
        wiced_rtos_get_semaphore(&button0_semaphore,WICED_WAIT_FOREVER);
        sendData('R', reg_LED1, 0);
    }
}

void application_start(void)
{
    wiced_init( );
    wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );

    wiced_result_t result;

    wiced_network_set_hostname("WICED001");

    wwd_wifi_get_mac_address(&myMac, WICED_STA_INTERFACE );

    // Use DNS to find the address.. if you can't look it up after 5 seconds then hard code it.
    WPRINT_APP_INFO(("DNS Lookup iotserver2\n"));
    result = wiced_hostname_lookup( "iotserver2", &serverAddress, 5000, WICED_STA_INTERFACE );
    if ( result == WICED_ERROR || serverAddress.ip.v4 == 0 )
    {
        WPRINT_APP_INFO(("Error in resolving DNS using hard coded address\n"));
        SET_IPV4_ADDRESS( serverAddress, MAKE_IPV4_ADDRESS( 198,51,  100,  3 ) );
    }
    else
    {
        WPRINT_APP_INFO(("iotserver2 IP : %u.%u.%u.%u\n\n", (uint8_t)(GET_IPV4_ADDRESS(serverAddress) >> 24),
                        (uint8_t)(GET_IPV4_ADDRESS(serverAddress) >> 16),
                        (uint8_t)(GET_IPV4_ADDRESS(serverAddress) >> 8),
                        (uint8_t)(GET_IPV4_ADDRESS(serverAddress) >> 0)));
     }

    WPRINT_APP_INFO(("MY MAC Address: "));
    WPRINT_APP_INFO(("%X:%X:%X:%X:%X:%X\r\n",
            myMac.octet[0], myMac.octet[1], myMac.octet[2],
            myMac.octet[3], myMac.octet[4], myMac.octet[5]));

    // Setup the Semaphore and Button Interrupt
    wiced_rtos_init_semaphore(&button0_semaphore); // the semaphore unlocks when the user presses the button
    wiced_rtos_init_semaphore(&button1_semaphore); // the semaphore unlocks when the user presses the button

    wiced_gpio_input_irq_enable(WICED_SH_MB0, IRQ_TRIGGER_FALLING_EDGE, button_isr0, NULL); // call the ISR when the button is pressed
    wiced_gpio_input_irq_enable(WICED_SH_MB1, IRQ_TRIGGER_FALLING_EDGE, button_isr1, NULL); // call the ISR when the button is pressed

    wiced_rtos_create_thread(&buttonUpdate, WICED_DEFAULT_LIBRARY_PRIORITY, "Button Update", buttonUpdateMain, TCP_CLIENT_STACK_SIZE, 0);
    wiced_rtos_create_thread(&buttonInquiry, WICED_DEFAULT_LIBRARY_PRIORITY, "Button Inquiry", buttonInquiryMain, TCP_CLIENT_STACK_SIZE, 0);
    WPRINT_APP_INFO(("Activated button threads..."));

}
