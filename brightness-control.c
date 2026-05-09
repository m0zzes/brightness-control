#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <dbus/dbus.h>


#define BACKLIGHT "/sys/class/backlight"
#define LEDS "/sys/class/leds"
#define B_CUR "brightness"
#define B_MAX "max_brightness"

void print_usage(char* argv0)
{
    printf("Usage: %s [-b <device>] [-l <device>] [-a <value>] [-s <value>] [-L] [-v] [-h]\n", argv0);
    printf("    -b <device>:    Selects a backlight device, prints current brightness level if neither -a or -s present\n");
    printf("    -l <device>:    Selects a LED device, prints current brightness level if neither -a or -s present\n");
    printf("    -a <value> :    Adds the value to the selected device brightness\n");
    printf("    -s <value> :    Subtracts the value from the selected device brightness\n");
    printf("    -L         :    Prints out the found devices\n");
    printf("    -v         :    Prints out program version\n");
    printf("    -h         :    Shows program usage\n\n");
    printf("    NOTE       :    Specifying both a backlight and LED is not permitted.\n");
    printf("               :    It is not possible to set a value higher then the maximum brightness for any selected device\n");
}

int read_value(char* path) {
    /*
     * Retrieves the current level-value
     * for the specified device
     * */

    FILE* fptr;
    fptr = fopen(path, "r");

    if (!fptr) { return -1; }

    char rbuffer[10];

    fgets(rbuffer, 10, fptr);

    return atoi(rbuffer);
}

int write_value(char* path, int value) {

    FILE* fptr;
    fptr = fopen(path, "w");

    if (!fptr) { return -1; }
   
    char str[10];
    sprintf(str, "%d", value);

    fprintf(fptr, str);
    
    return 1;
}

int dbus_set_brightness(char* subsystem, char* device, uint32_t value) {
    /* Sets the brightness by calling the dbus API-method
     * SetBrightness with the correct arguments
     * */    

    DBusConnection* connection = NULL;
    DBusError dbus_error; 
    DBusMessage *msg = NULL;
    DBusMessage *reply = NULL;

    dbus_error_init(&dbus_error);
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);

    if (dbus_error_is_set(&dbus_error)) {return -1;}
    
    msg = dbus_message_new_method_call(
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/auto",
        "org.freedesktop.login1.Session",
        "SetBrightness"        
    );


    printf("%s, %s, %d\n", subsystem, device, value);

    if (!dbus_message_append_args(
        msg,
        DBUS_TYPE_STRING, &subsystem,
        DBUS_TYPE_STRING, &device,
        DBUS_TYPE_UINT32, &value,
        DBUS_TYPE_INVALID
    )) {return -1;}
    
    reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &dbus_error);

    return 1;
}

int read_device_values(char* directory, char* device, int* result) {
   /* Reads the values for brightness and maximum-brightness 
    * given the specifed device name.
    *
    * Values are stored in passed array of integers.
    * */ 

    size_t path_cur_size = strlen(directory) + strlen(device) + strlen(B_CUR) + 3;
    size_t path_max_size = strlen(directory) + strlen(device) + strlen(B_MAX) + 3;
    
    char path_cur[path_cur_size];
    char path_max[path_max_size];

    snprintf(path_cur, path_cur_size, "%s/%s/%s", directory, device, B_CUR);
    snprintf(path_max, path_max_size, "%s/%s/%s", directory, device, B_MAX);

    int brightness = read_value(path_cur);
    int max_brightness = read_value(path_max);

    if (brightness < 0 || max_brightness < 0) {
        return -1;
    }

    result[0] = brightness;
    result[1] = max_brightness;

    return 0;
}


void change_device_brightness(char* directory, char* device, int delta) { 
   /* Changes the device brightness by adding the delta to the current
    * value. If delta is < 0 the brightness gets dimmer. Brightness
    * will never exceed the maximum value for the specified device
    * */ 

    int values[2];
    int result = read_device_values(directory, device, values);
    if (result == -1) {
        printf("Could not read device %s values at %s\n", device, directory);
        return; 
    }

    int current_brightness = values[0];
    int max_brightness = values[1]; 
    int new_brightness = current_brightness + delta;

    if (new_brightness >= max_brightness) {
        new_brightness = max_brightness;
    }
    else if (new_brightness <= 0) {
        new_brightness = 100;
    }

    // TODO: This is a duplicate from read_device_values()
    // fix a general help-function to avoid duplicates
    size_t path_size = strlen(directory) + strlen(device) + strlen(B_CUR) + 3;
    char path[path_size];
    snprintf(path, path_size, "%s/%s/%s", directory, device, B_CUR);

    //if (write_value(path, new_brightness) == -1) {
    //    printf("Could not change brightness at %s, check permissions\n", path); 
    //}
    
    char* subsystem;
    if (strcmp(directory, BACKLIGHT) == 0) {
        subsystem = "backlight";
    }
    else if(strcmp(directory, LEDS) == 0) {
        subsystem = "leds";    
    }

    if  (dbus_set_brightness(subsystem, device, new_brightness) == -1) {return;}
    
    return;
}

void list_devices(char* path) {
    /* Lists the devices found in the give path,
     * in reality it will only list the directories or files
     * in the path, but in our case, these will be the available
     * lights (backlight and LEDs) that are controllable
     * */

    struct dirent* de;
    DIR* dr = opendir(path);

    while ((de = readdir(dr)) != NULL) {
        
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0){continue;}

        printf("%s\n", de->d_name);
    }
}

void device_status(char* directory, char* device) {

    int values[2];
    int result = read_device_values(directory, device, values);


    if (result == -1) {
        printf("device [%s] not found\n", device);
        return;
    }

    printf("%s, brightness=%d, max_brightness=%d\n", device, values[0], values[1]);

}

int main(int argc, char** argv){

    int opt = 1;
    int add = -1;
    int subtract = -1;
    int value = 0;
    char* backlight_device = NULL;
    char* led_device = NULL;

    while ((opt = getopt(argc, argv, "b:l:a:s:vhL")) != -1) {
        
        switch(opt) {
        
            case 'b':
                backlight_device = optarg;
                break;
            case 'l':
                led_device = optarg;
                break;
            case 'a':
                add = atoi(optarg);
                value += add;
                break;
            case 's':
                subtract = atoi(optarg);
                value -= subtract;
                break;
            case 'v':
                printf("Version 0.0\n");
                return 0;
            case 'L':
                list_devices(BACKLIGHT);
                list_devices(LEDS);
                return 0;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (backlight_device == NULL && led_device == NULL) {
        printf("No device selected, see usage [-h]\n");
        return 0;
    }
    else if(backlight_device != NULL && led_device != NULL) {
        printf("Backlight and LED devices cannot be selected at the same time, see usage [-h]\n");
        return 0;
    }
    
    // If neither -a or -s option is used, print device status
    if (add == -1 && subtract == -1) {
        if (backlight_device == NULL) {
            device_status(LEDS, led_device);
        }
        else{
            device_status(BACKLIGHT, backlight_device);
        }
        return 0;
    }


    if (backlight_device == NULL) {
       change_device_brightness(LEDS, led_device, value); 
    }

    else if (led_device == NULL) {
        change_device_brightness(BACKLIGHT, backlight_device, value);
    }

    return 0;
}
