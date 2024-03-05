#include "firebase.h"
static void pushData(const char *url, const char* data) {
    CURL *curl;
    CURLcode res;
    
    curl = curl_easy_init();
    if(curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        
	res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        
	curl_easy_cleanup(curl);
    }
}

static void removeData(const char *url) 
{
    CURL *curl;
    CURLcode res;
    
    curl = curl_easy_init();
    if(curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        
        curl_easy_cleanup(curl);
    }
}
void db_add_node(struct LoRa_node LoRa) 
{
    db_update_data(LoRa);
}

void db_remove_node(struct LoRa_node LoRa) 
{
    char url[200];
    sprintf(url, "https://lora-mesher-default-rtdb.firebaseio.com/%u.json?auth=%s", LoRa.id, firebaseKey);
    removeData(url);
}

void db_update_data(struct LoRa_node LoRa) 
{
    char url[200];
    char data[100];
    const char *mode = (LoRa.current_mode == MODE_AUTO) ? "Auto" : "Manual";
    sprintf(data, "{\"Light intensity\": %u, \"Illuminance\": %u, \"Source Voltage\": %.2f, \"Source Current\": %.2f}", LoRa.light_sensor_value, LoRa.illuminance, LoRa.voltage, LoRa.current);
    sprintf(url, "https://lora-mesher-default-rtdb.firebaseio.com/%u.json?auth=%s", LoRa.id, firebaseKey);
    pushData(url, data);
}
