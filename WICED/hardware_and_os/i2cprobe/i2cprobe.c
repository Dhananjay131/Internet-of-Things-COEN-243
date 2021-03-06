//------------------------------------------------------------
// SCU's Internet of Things Research Lab (SIOTLAB)
// Santa Clara University (SCU)
// Santa Clara, California
//------------------------------------------------------------

// This application is based on the Cypress WICED platform

//------------------------------------------------------------


// When the button on the base board is pressed, the firmware will scan all I2C addresses
// and will report any addresses that ACK'd
#include "wiced.h"

#define RETRIES (1)
#define MIN_I2C_ADDRESS (0x01)
#define MAX_I2C_ADDRESS (0x7B)

volatile wiced_bool_t buttonPress = WICED_FALSE;

/* Interrupt service routine for the button */
void button_isr(void* arg)
{
	buttonPress = WICED_TRUE;
}

/* Main application */
void application_start( )
{
	uint8_t i2cAddress = 0x00;

	wiced_init();	/* Initialize the WICED device */

    wiced_gpio_input_irq_enable(WICED_SH_MB1, IRQ_TRIGGER_FALLING_EDGE, button_isr, NULL); /* Setup interrupt */

    /* Setup I2C master device structure */
    wiced_i2c_device_t i2cDevice = {
    	.port = WICED_I2C_2,
		.address = i2cAddress,
		.address_width = I2C_ADDRESS_WIDTH_7BIT,
		.speed_mode = I2C_STANDARD_SPEED_MODE
    };

    while ( 1 )
    {
    	if(buttonPress)
    	{
    	    WPRINT_APP_INFO(("\n"));
    		/* Scan for I2C Devices */
    		for(i2cAddress=MIN_I2C_ADDRESS; i2cAddress<=MAX_I2C_ADDRESS; i2cAddress++)
    		{
				i2cDevice.address=i2cAddress;	/* Put new address in the I2C device structure */
				wiced_i2c_init(&i2cDevice);		/* Initialize I2C with the new address */
				if(wiced_i2c_probe_device(&i2cDevice, RETRIES) == WICED_TRUE) /* Device was found at this address */
				{
					WPRINT_APP_INFO(("Device Found at: 0x%02X\n", i2cAddress)); /* Print data to terminal */
				}
    		}

    		buttonPress = WICED_FALSE; /* Reset flag for next button press */
    	}
    }
}
