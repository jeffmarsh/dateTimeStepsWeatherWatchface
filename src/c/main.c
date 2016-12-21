#include <pebble.h>

// Total Steps (TS)
#define TS 1
// Total Steps Default (TSD)
#define TSD 1

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_calendar_layer;

static TextLayer *s_weather_layer;
static TextLayer *s_message_layer;
static Layer *s_canvas_layer;

static BitmapLayer *battery_bitmap_layer;
static GBitmap *BatteryCharge;
static GBitmap *Battery100;
static GBitmap *Battery75;
static GBitmap *Battery50;
static GBitmap *Battery25;
static GBitmap *Battery0;

int topOffset = 10;

int currX, currY, currZ = 0;
int prevX = 0;
int absX, absY = 0;
int lastX, lastY, lastZ = 0;

const int ACCEL_STEP_MS = 475;
const int PED_ADJUST = 2;

//int deltaX = 35;
//int deltaY, deltaZ = 185;

//int deltaXTemp, deltaYTemp, deltaZTemp =0;
int X_DELTA_TEMP, Y_DELTA_TEMP, Z_DELTA_TEMP = 0;
int X_DELTA = 35;
int Y_DELTA, Z_DELTA = 185;
int YZ_DELTA_MIN = 175;
int YZ_DELTA_MAX = 195; 

bool validX, validY, validZ = false;
bool stepCounting = false;
bool Xinterest, Yinterest, Binterest = false;
long pedometerCount = 0;
int validInc = 0;
int sensativity = 3;

int graphing_x = 0;

bool did_pebble_vibrate = false;

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // A single click has just occured
  pedometerCount = 0;
  static char buf[] = "123456890abcdefghijkl";
  snprintf(buf, sizeof(buf), "%ld", pedometerCount);
  text_layer_set_text(s_message_layer, buf);
  APP_LOG(APP_LOG_LEVEL_INFO, "Button was clicked");
}

static void click_config_provider(void *context) {
  ButtonId id = BUTTON_ID_SELECT;  // The Select button

  window_single_click_subscribe(id, select_click_handler);
}

static void handle_battery(BatteryChargeState charge_state) {
  if (charge_state.is_charging) {
    bitmap_layer_set_bitmap(battery_bitmap_layer, BatteryCharge);
  } else {
    if (charge_state.charge_percent > 75){
      bitmap_layer_set_bitmap(battery_bitmap_layer, Battery100);
    } else if (charge_state.charge_percent >50){
      bitmap_layer_set_bitmap(battery_bitmap_layer, Battery75);
    } else if (charge_state.charge_percent > 25){
      bitmap_layer_set_bitmap(battery_bitmap_layer, Battery50);
    } else if (charge_state.charge_percent > 10){
      bitmap_layer_set_bitmap(battery_bitmap_layer, Battery25);
    } else if (charge_state.charge_percent > 0 || charge_state.charge_percent == 0){
      bitmap_layer_set_bitmap(battery_bitmap_layer, Battery0);
    }
  }
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  
  // Copy date into buffer from tm structure
  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%m.%d.%y", tick_time);
  
  // Show the date
  text_layer_set_text(s_calendar_layer, date_buffer);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "drawing separators");
  graphics_context_set_stroke_width(ctx, 5);
  int topX = topOffset+40;
  int secondX = topX+40;
  int lastX = secondX+40;
  int startY = 3;
  int endY = 140;
  graphics_draw_line(ctx, GPoint(startY, topX), GPoint(endY, topX));
  graphics_draw_line(ctx, GPoint(startY, secondX), GPoint(endY, secondX));
  graphics_draw_line(ctx, GPoint(startY, lastX), GPoint(endY, lastX));

}

void autoCorrectZ(){
	if (Z_DELTA > YZ_DELTA_MAX){
		Z_DELTA = YZ_DELTA_MAX; 
	} else if (Z_DELTA < YZ_DELTA_MIN){
		Z_DELTA = YZ_DELTA_MIN;
	}
}

void autoCorrectY(){
	if (Y_DELTA > YZ_DELTA_MAX){
		Y_DELTA = YZ_DELTA_MAX; 
	} else if (Y_DELTA < YZ_DELTA_MIN){
		Y_DELTA = YZ_DELTA_MIN;
	}
}

static void pedometer_update(){
  X_DELTA_TEMP = abs(abs(currX) - abs(lastX));
		if (X_DELTA_TEMP >= X_DELTA) {
			validX = true;
		}
		Y_DELTA_TEMP = abs(abs(currY) - abs(lastY));
		if (Y_DELTA_TEMP >= Y_DELTA) {
			validY = true;
			if (Y_DELTA_TEMP - Y_DELTA > 200){
				autoCorrectY();
				Y_DELTA = (Y_DELTA < YZ_DELTA_MAX) ? Y_DELTA + PED_ADJUST : Y_DELTA;
			} else if (Y_DELTA - Y_DELTA_TEMP > 175){
				autoCorrectY();
				Y_DELTA = (Y_DELTA > YZ_DELTA_MIN) ? Y_DELTA - PED_ADJUST : Y_DELTA;
			}
		}
		Z_DELTA_TEMP = abs(abs(currZ) - abs(lastZ));
		if (abs(abs(currZ) - abs(lastZ)) >= Z_DELTA) {
			validZ = true;
			if (Z_DELTA_TEMP - Z_DELTA > 200){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			} else if (Z_DELTA - Z_DELTA_TEMP > 175){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			}
		}
}

void resetUpdate() {
	lastX = currX;
	lastY = currY;
	lastZ = currZ;
	validX = false;
	validY = false;
	validZ = false;
}

void update_ui_callback() {
	if ((validX && validY && !did_pebble_vibrate) || (validX && validZ && !did_pebble_vibrate)) {
		pedometerCount++;
    APP_LOG(APP_LOG_LEVEL_INFO, "Pedcount:%ld", pedometerCount);
    
		static char buf[] = "123456890abcdefghijkl";
		snprintf(buf, sizeof(buf), "%ld", pedometerCount);
    text_layer_set_text(s_message_layer, buf);
    
  }
  resetUpdate();
}

  
static void accel_callback(void *data) {
  AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);
  
  currX = accel.x;
	currY = accel.y;
	currZ = accel.z;
  
  did_pebble_vibrate = accel.did_vibrate;

	pedometer_update();
	update_ui_callback();
  
  app_timer_register(ACCEL_STEP_MS, accel_callback, NULL);
}

static void main_window_load(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "Main window setup");
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create canvas layer
  s_canvas_layer = layer_create(bounds);
  
  // Load graphics
  //kitty = gbitmap_create_with_resource(RESOURCE_ID_KITTY);
  BatteryCharge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);
  Battery100 = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_100);
  Battery75 = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_75);
  Battery50 = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_50);
  Battery25 = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_25);
  Battery0 = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_EMPTY);
  
  // Time layer
  s_time_layer = text_layer_create(GRect(0, topOffset-3, bounds.size.w - 5, 58));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorMagenta);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  
   // Battery Layer  
  battery_bitmap_layer = bitmap_layer_create(GRect(3, topOffset-10, 20, 38));
  bitmap_layer_set_alignment(battery_bitmap_layer, GAlignBottomRight );
  
  // Calendar layer
  s_calendar_layer = text_layer_create(GRect(0, topOffset+40, bounds.size.w, 30));
  text_layer_set_background_color(s_calendar_layer, GColorClear);
  text_layer_set_text_color(s_calendar_layer, GColorBlack);
  text_layer_set_text(s_calendar_layer, "07.07.16");
  text_layer_set_text_alignment(s_calendar_layer, GTextAlignmentCenter);
  text_layer_set_font(s_calendar_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  
  // Weather Layer
  s_weather_layer = text_layer_create(GRect(0, topOffset+80, bounds.size.w, 30));
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorBlack);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_font(s_weather_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(s_weather_layer, "...");
  
  // Message Layer
  s_message_layer = text_layer_create(GRect(0, topOffset+120, bounds.size.w - 5, 30));
  text_layer_set_background_color(s_message_layer, GColorClear);
  text_layer_set_text_color(s_message_layer, GColorMagenta);
  text_layer_set_text_alignment(s_message_layer, GTextAlignmentCenter);
  text_layer_set_font(s_message_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  
  // Put it all together
  
  battery_state_service_subscribe(handle_battery);
  accel_data_service_subscribe(0, NULL);
  app_timer_register(ACCEL_STEP_MS, accel_callback, NULL);
 
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(battery_bitmap_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_calendar_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_message_layer));
  handle_battery(battery_state_service_peek());
 
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  
  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
  
  // If all data is available, use it
  if(temp_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%d", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
  }
   // Assemble full string and display
  snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temperature_buffer, conditions_buffer);
  //snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", temperature_buffer);
  text_layer_set_text(s_weather_layer, weather_layer_buffer);
    
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  gbitmap_destroy(BatteryCharge);
  gbitmap_destroy(Battery100);
  gbitmap_destroy(Battery75);
  gbitmap_destroy(Battery50);
  gbitmap_destroy(Battery25);
  gbitmap_destroy(Battery0);
  bitmap_layer_destroy(battery_bitmap_layer);
  text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_message_layer);
}


static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_set_click_config_provider(s_main_window, click_config_provider);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
  
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
 