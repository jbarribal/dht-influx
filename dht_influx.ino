/**
 * Basic Write Example code for InfluxDBClient library for Arduino
 * Data can be immediately seen in a InfluxDB UI: wifi_status measurement
 * Enter WiFi and InfluxDB parameters below
 *
 * Measures signal level of the actually connected WiFi network
 * This example supports only InfluxDB running from unsecure (http://...)
 * For secure (https://...) or Influx Cloud 2 use SecureWrite example
 **/

#include <Arduino.h>
//#include <WiFiMulti.h>
#include <ESP8266WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <DHT.h>
#include <DHT_U.h>

// Project Macros
#define WIFI_SSID             "Barribal_WiFi"
#define WIFI_PASSWORD         "Welcome@123"
#define INFLUXDB_URL          "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN        "V1Vb-6M60OA_RhIXv4zx0O5V_uq03OxBIHpYfT9e9KXVuCRDrdnyuDUKCkbMXp5aS1aJDoeNV7tFSJqZl4Rv0Q=="
#define INFLUXDB_ORG          "dev"
#define INFLUXDB_BUCKET       "arduino"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define DEVICE                          "ESP8266"

// DHT Sensor Configuration Macros
#define DHTPIN                          12
#define DHTTYPE                         DHT22

#define DHT11_REFRESH_TIME              (5000u)     // 5 seconds
#define INFLUXDB_SEND_TIME              (60000u)    // 1 minute

#define SENSOR_BUFFER_SIZE              (INFLUXDB_SEND_TIME/DHT11_REFRESH_TIME)

// Priavate Variables

// InfluxDB client instance with preconfigured InfluxCloud Certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor("Sensor_Data");

//WiFiMulti wifiMulti;

static uint8_t dht11_temperature = 0;
static uint8_t dht11_humidity = 0u;
static uint8_t temp_buffer[SENSOR_BUFFER_SIZE] = { 0 };
static uint8_t humidity_buffer[SENSOR_BUFFER_SIZE] = { 0 };
static uint16_t sensor_buffer_idx = 0;
DHT dht(DHTPIN, DHTTYPE);

// Task Time related Variables
static uint32_t dht_refresh_timestamp = 0u;
static uint32_t influxdb_send_timestamp = 0u;

// Private Function Prototypes
static void System_Init( void );
static void WiFi_Setup( void );
static void DHT11_TaskInit( void );
static void DHT11_TaskMng( void );
static void InfluxDB_TaskInit( void );
static void InfluxDB_TaskMng( void );

static uint8_t Get_HumidityValue( void );
static uint8_t Get_TemperatureValue( void );

void setup()
{
  System_Init();
  WiFi_Setup();
  DHT11_TaskInit();
  InfluxDB_TaskInit();
}

void loop()
{
  DHT11_TaskMng();
  InfluxDB_TaskMng();
}

// Private Function Definitions
static void System_Init( void )
{
  Serial.begin( 115200 );
  // TODO: XS
}

static void WiFi_Setup( void )
{
  // Connect WiFi
  Serial.println("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
    }
  Serial.println(WiFi.localIP());
}

static void DHT11_TaskInit( void )
{
  dht.begin();
  // delay(2000);
  dht_refresh_timestamp = millis();
}

static void DHT11_TaskMng( void )
{
  uint32_t now = millis();
  float temperature, humidity;
  if( now - dht_refresh_timestamp >= DHT11_REFRESH_TIME )
  {
    dht_refresh_timestamp = now;
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    temperature = dht.readTemperature();
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature) ) 
    {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    else
    {
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.print(F("%  Temperature: "));
      Serial.print(temperature);
      Serial.println(F("°C "));
      // store this in the global variables
      dht11_humidity = (uint8_t)humidity;
      dht11_temperature = (uint8_t)temperature;
      temp_buffer[sensor_buffer_idx] = dht11_temperature;
      humidity_buffer[sensor_buffer_idx] = dht11_humidity;
      sensor_buffer_idx++;
      if( sensor_buffer_idx >= SENSOR_BUFFER_SIZE )
      {
        sensor_buffer_idx = 0;
      }
    }
  }
}

static void InfluxDB_TaskInit( void )
{
  // Add constant tags - only once
  sensor.addTag("device", DEVICE);

  // Accurate time is necessary for certificate validation & writing in batches
  // For the fastest time sync find NTP servers in your area: 
  // https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) 
  {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  }
  else
  {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

static void InfluxDB_TaskMng( void )
{
  uint32_t now = millis();
  if( now - influxdb_send_timestamp >= INFLUXDB_SEND_TIME )
  {
    influxdb_send_timestamp = now;
    // Serial.print("Average Humdity = ");
    // Serial.println( Get_HumidityValue() );
    // Serial.print("Average Temperature = ");
    // Serial.println( Get_TemperatureValue() );

    // Store measured value into point
    sensor.clearFields();
    // Report RSSI of currently connected network
    sensor.addField( "rssi", WiFi.RSSI() );
    // add temperature and humidity values also
    sensor.addField( "temperature", Get_TemperatureValue() );
    sensor.addField( "humidity", Get_HumidityValue() );
    
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(client.pointToLineProtocol(sensor));
    // If no Wifi signal, try to reconnect it
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Wifi connection lost");
    }
    // Write point
    if (!client.writePoint(sensor))
    {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  }
}

static uint8_t Get_HumidityValue( void )
{
  uint16_t idx = 0u;
  uint32_t temp = 0u;
  for( idx=0; idx<SENSOR_BUFFER_SIZE; idx++ )
  {
    temp = temp + humidity_buffer[idx];
  }
  temp = temp/SENSOR_BUFFER_SIZE;
  return (uint8_t)temp;
}

static uint8_t Get_TemperatureValue( void )
{
  uint16_t idx = 0u;
  uint32_t temp = 0u;
  for( idx=0; idx<SENSOR_BUFFER_SIZE; idx++ )
  {
    temp = temp + temp_buffer[idx];
  }
  temp = temp/SENSOR_BUFFER_SIZE;
  return (uint8_t)temp;
}
