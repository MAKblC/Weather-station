var sheet_id = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"; // ID скрипта
var sheet_name = "ESP_Sensor"; // имя листа
function doGet(e){
var ss = SpreadsheetApp.openById(sheet_id);
var sheet = ss.getSheetByName(sheet_name);
var time = String(e.parameter.time);
var svet = Number(e.parameter.svet);
var uv = Number(e.parameter.uv);
var temp = Number(e.parameter.temp);
var speed = Number(e.parameter.speed);
var rain = Number(e.parameter.rain);
var windV = Number(e.parameter.windV);
var press = Number(e.parameter.press);
var hum = Number(e.parameter.hum);
sheet.appendRow([time,svet,uv,temp,speed,rain,windV,press,hum]);
}
