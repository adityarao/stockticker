#include <SPI.h>
#include "LedMatrix.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define ssid  "harisarvottama"
#define password ""

#define NUMBER_OF_DEVICES 5
#define CS_PIN D4

#define BASETICKER "NYSE:NOW"

bool resetFromWeb = false;

ESP8266WebServer server;

LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CS_PIN);
int x = 0;

unsigned long tGetTicker = 0, tLEDMatrixInterval = 0;

const char *tickerHost = "www.google.com";
const int tickerPort = 80;

String tickerSymbols = BASETICKER;


  
void setup() {
	Serial.begin(115200);

	tGetTicker = tLEDMatrixInterval = millis();

	resetFromWeb = false;

	ledMatrix.init();
	ledMatrix.setText("Getting tickers from google ..");

	connectToWiFi();

	String str = getTickerJson(tickerSymbols);
	ledMatrix.setNextText(str);
}

void loop() {
	server.handleClient();

	if(millis() - tLEDMatrixInterval > 10) { 
		ledMatrix.clear();
		ledMatrix.scrollTextLeft();
		ledMatrix.drawText();
		ledMatrix.commit();

		tLEDMatrixInterval = millis();
	}

	if((millis() - tGetTicker > 120000) || resetFromWeb) {
		String str = getTickerJson(tickerSymbols);

		Serial.print("Ticker : ");
		Serial.println(str);
		
		ledMatrix.setNextText(str);

		tGetTicker = millis();
		resetFromWeb = false;
	}

}

// connects to Wifi and sets up the WebServer !
void connectToWiFi() {
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());

	// start the server to get the ticker symbol
	server.on("/", [](){server.send(200, "text/plain", "Welcome to NowTick!!");});

	server.on("/list", []() {
		server.send(200, "text/plain", "OK: list of symbols: " + tickerSymbols);
	}); 

	server.on("/manage", []() {
	  String add = server.arg("add");
	  
	  if(add != "") { 

	  	// convert to uppercase for brevity 
	  	add.toUpperCase();

	  	Serial.print("Value of add: ");
	  	Serial.println(add);

	  	if(add.indexOf(":") > 0) {
	  		// no check here ! be careful :) 

	  		if(tickerSymbols.indexOf(add) > -1) {
				server.send(400, "text/plain", "Warning: Ticker symbol already listed : " + add);
	  		} else {
		  		tickerSymbols = tickerSymbols + "," + add;
				server.send(202, "text/plain", "Accepted: Added ticker symbol: " + add);
				resetFromWeb = true; // needs a refresh	  			
	  		}
		} else {
			server.send(400, "text/plain", "ERROR: Ticker symbols should  be <stockExchange>:<TickerSymbol> you sent : " + add);
		}
	  }

	  String remove = server.arg("remove"); 

	  if(remove != "") { 

	  	// deal with upper case for brevity
	  	remove.toUpperCase();

	  	Serial.print("Value of remove: ");
	  	Serial.println(remove);	  	

	  	// if the symbol is not found or malformed
	  	if(tickerSymbols.indexOf(remove) < 0){
	  		server.send(400, "text/plain", "ERROR: did not find the ticker symbol : " + remove);
	  	} else {
		  	if(remove.indexOf(":") > 0) {
		  		// no check here ! be careful :) 
		  		tickerSymbols.replace(remove, "");
		  		// replace 2 commas by one if a middle symbol is removed
		  		tickerSymbols.replace(",,", ",");

		  		// , at the begining - remove it
		  		if(tickerSymbols.startsWith(","))
		  			tickerSymbols = tickerSymbols.substring(1);
		  		// , at the end - remove it too
		  		if(tickerSymbols.endsWith(","))
		  			tickerSymbols.remove(tickerSymbols.length()-1);

		  		// needs a reset now
				resetFromWeb = true;

		  		// if the last ticker removed too
		  		if(tickerSymbols == "") {
		  			tickerSymbols = BASETICKER;
		  			server.send(202, "text/plain", "Reset the ticker back to " + tickerSymbols);
		  		} else {
					server.send(202, "text/plain", "Accepted: removed symbol: " + remove);
		  		}

			} else {
				server.send(400, "text/plain", "ERROR: Ticker symbols to be removed should  be <stockExchange>:<TickerSymbol> you sent : " + remove);
			}
		}
	  }	  

	});  
	// start the server
	Serial.println("Starting the server ... ");
	server.begin(); 
}

String getTickerJson(String symbol) {

	Serial.println("Trying to connect to the google!....");
	// Use WiFiClient class to create TLS connection
	WiFiClient client;
	if (!client.connect(tickerHost, 80)) {
		Serial.print("Failed to connect to :");
		Serial.println(tickerHost);
		return "error";
	}

	Serial.println("connected !....");

	String url = "/finance/info?q=" + symbol;

	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
	           "Host: " + tickerHost + "\r\n" +
	           "User-Agent: ESP8266\r\n" +
	           "Content-Type: application/json\r\n" +
	           "Accept: */*\r\n" +
	           "Connection: close\r\n\r\n");


	// bypass HTTP headers
	while (client.connected()) {
		String line = client.readStringUntil('\n');
		//Serial.println( "Header: " + line );
		if (line == "\r") {
		  break;
		}
	}
	//  read body length
	int bodyLength = 0;
	while (client.connected()) {
		String line = client.readStringUntil('\n');
		Serial.println( "Body length: " + line );
		bodyLength = line.toInt();
		break;
	}

	String jsonMsg; 

	while (client.connected()) {
	 	String line = client.readStringUntil('\n');

		if(line == "\r")
			break;

		// Basic parsing - not using JSON library to keep it lite !
		line = line.substring(line.indexOf(",")+1);
		line.replace("\":\"", "**");
		line.replace("\"", "");
		line.replace(" : ", "**");
		line.replace(" :", "**");
		line.replace(": ", "**");
		//Serial.println(line);

		// ticker symbol
		if(line.startsWith("t**")) {
			jsonMsg += line.substring(3) + ":";
		}

		// ticker value
		if(line.startsWith("l**")) {
			jsonMsg += line.substring(3);
		}

		// ticker change
		if(line.startsWith("c**")) {
			jsonMsg += "(" + line.substring(3) + ") -- ";
		}
	}

  	return jsonMsg;
}