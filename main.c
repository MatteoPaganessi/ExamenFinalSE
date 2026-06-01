//Implemente una funcion llamada spi_bus_init que configure el bus spi segun el modo que requiere el MCP4132 para comunicarse correctamente
void spi_bus_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num   = PIN_23,
        .miso_io_num   = PIN_19,
        .sclk_io_num   = PIN_18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000,  
        .mode           = 0,        
        .spics_io_num   = PIN_5,   
        .queue_size     = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev, &s_mcp);
    ESP_LOGI(MCP4132, "SPI listo");
}
{
    spi_bus_config_t bus = {
        .mosi_io_num   = PIN_23,
        .miso_io_num   = PIN_19,
        .sclk_io_num   = PIN_18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000,  
        .mode           = 0,        
        .spics_io_num   = PIN_5,   
        .queue_size     = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev, &s_mcp);
    ESP_LOGI(MCP4132, "SPI listo");
}
// Implemente la funcion que permita escribir un valore en un registro deseado.
// La funcion debe de tomar como parametro el valor del registro y la direccion.
void mcp4132_write_register(uint8_t direccion, uint8_t valor)
{
    if (valor > 128) valor = 128;
    uint8_t tx[2];
    tx[0] = (direccion << 4) | 0x00;   
    tx[1] = valor;                     

    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
    };
    spi_device_transmit(s_mcp, &t);
}
//Implemente la funcion mcp4132_read_register que permita leer el contenido de un registro deseado del mcp.
//La funcion debe tomar como parametro la direccion del registro que se desea leer y retornar el valor leido.
//Tenga en cuenta que la funcion debe extraer y retornar unicamente los datos utiles de la respuesta recibida mediante las mascaras de bits adecuadas
uint8_t mcp4132_read_register(uint8_t direccion)
{
    uint8_t tx[2];
    uint8_t rx[2];
    tx[0] = (direccion << 4) | 0x0C;  
    tx[1] = 0x00;                      
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
};
    spi_device_transmit(s_mcp, &t);

    return rx[1] & 0xFF;  /*Retorna solo lo importante*/

//Implemente la funcion mcp4132_set_wiper que permita escribir directamente un valor N en el registro Wiper 0.
//La funcion debe escribir en los registros correspondientes(para ello use la funcion de escritura de registros implementada en el punto anterior)
//La funcion debe recibir parametro un valor entre 0-128 y validar que no se trate de escribir un valor por fuera de este rango
void mcp4132_set_wiper(uint8_t valor)
{
    if (valor > 128) valor = 128; 
    mcp4132_write_register(0x00, valor); 
}

//Implemente una funcion mcp4132_set_cutoff_frecuency que configura el MCP de modo que la resistencia Rwb para que el filtro opere a la frecuencia de corte deseada.
//La funcion debe tener como parametro la frecuencia de corte en Hx deseada, para ello, calcule el valor de N que produce la resistencia Rwb mas adecuada para la frecuencia objetivo usando las ecuaciones
// f=1/(2*pi*Rwb*C) y Rwb = ((N/128)*10000)+75 y el capacitor es de 10nF
void mcp4132_set_cutoff_frequency(float frecuencia_corte)
{
    const float C = 10e-9;
    const float R_min = 75;
    const float R_max = 10000;
    float Rwb = 1 / (2 * 3.1415 * frecuencia_corte * C);
    if (Rwb < R_min) Rwb = R_min;
    if (Rwb > R_max) Rwb = R_max;
    uint8_t N = (uint8_t)(((Rwb - R_min) / (R_max - R_min)) * 128);
    mcp4132_set_wiper(N);
}
// Dentro del void app_main(void) Configure el ADC a 12 bits y el driver UART a 115200 baudios
//Escriba un codigo que permita muestrear la señal del geofono con una frecuencia de muestreo de 1kHz, para ello use un timer y configurelo adecuadamente
//Si en algun momento la señal analogica supero los 1.4V, debera escribirse un valor de N95 en el registro del wiperm si es menor a 0.9V
//Debera escribirse un valor de N42, para ello use las funciones de manejo del wiper desarrolladas en el numeral anterior y cada vez que se hagaese cambio, debe enviarse por UART un mensaje indicando al operario el valor actual del wiper
void app_main(void)
{
    // Configuracion ADC a 12 bits
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

     //UART driver
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .name = "sampling_timer"
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, 1000); // 1ms = 1kHz


    void timer_callback(void *arg)
    {
        int adc_reading = adc1_get_raw(ADC1_CHANNEL_0);
        float voltage = adc_reading * (3.3 / 4095.0); 
        if (voltage > 1.4)
        {
            mcp4132_set_wiper(95);
            uart_write_bytes(UART_NUM_1, "Wiper set to N95\n", 17);
        }
        else if (voltage < 0.9)
        {
            mcp4132_set_wiper(42);
            uart_write_bytes(UART_NUM_1, "Wiper set to N42\n", 17);
        }
    }
}






