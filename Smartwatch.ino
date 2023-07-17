#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include "RTClib.h"
#include <DHT.h>
#include <SPI.h>
#include "MAX30100_PulseOximeter.h"
#include <time.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     16
#define OLED_CS     5
#define OLED_RESET  17

#define BACK 15     //Botón 1
#define DOWN 2      //Botón 2
#define UP 4        //Botón 3
#define FORWARD 27  //Botón 4

#define REPORTING_PERIOD_MS     1000 //Para oximetro, tomar valores cada segundo

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#define DHTPIN 14     // Digital pin connected to the DHT sensor

#define DHTTYPE    DHT22     // DHT 22 (AM2302)

//-----------------DHT-----------------
DHT dht(DHTPIN, DHTTYPE); //Modulo DHT

//-----------------RTC-----------------
RTC_DS3231 rtc; //Módulo RTC
int second, minute, hour, day, month, year;
char dayOfWeek[12];
char days[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
unsigned long alarmActivation = 0; //Momento de activación de la alarma, en milisegundos
int horaAlarma, minutoAlarma, segundoAlarma;
int opcionConf; //Elegir configurar hora, minuto o segundo
int comienzoConfAlarma = 0; //Variable para definir que se está configurando la alarma
int iniciar_oxi = 0; //Variable para definir si se está empleando el oximetro

//-----------------Menu-----------------
String menu[4];
int menuActual = 0; //Menú actual, antes de ejecutar
int menuAnterior = 0; //Guardamos el menú donde estaba para volver al acabar ejecución
int cambio = 0; //Cambio significa a cambio de contexto. Esto puede ser cambiar de pantalla o acceder a una de estas (ejecutarla)
int ejecutar = 0; //Toca pasar poner en ejecución una aplicación
int alarmaBloqueo = 0; //Bloquea para que no se salga de la alarma

//-----------------Botones-----------------
bool upLastState = HIGH; //Para botones
bool downLastState = HIGH;

//-----------------Oximeter-----------------
PulseOximeter pox;

uint32_t tsLastReport = 0; //Utilizado para medición de pulsaciones
//-----------------Funciones
void menuCreacion(){ //Se crea un menú con 4 opciones a elegir
  menu[0] = "Hora y Fecha";
  menu[1] = "Alarma";
  menu[2] = "Temperatura/Humedad";
  menu[3] = "Frecuencia/Oxigeno";
}

void actualizarMenu(){ //Utilizado para modificar el menú según los botones utilizados, es decir navegación, acceder o retroceso respecto a una pantalla
  if(alarmaBloqueo == 0){
    if(digitalRead(BACK) == HIGH){ //Si estoy en cualquier parte del menú y acciono retroceder, vuelvo a la opción de hora y fecha
      delay(200);
      if(ejecutar == 0){
        menuActual = 0;
      }else{
        menuActual = menuAnterior; //Si estoy en ejecución (menos alarma) y acciono retroceder, vuelvo a la selección
        ejecutar = 0;
        iniciar_oxi = 0;
      }
      cambio = 1; 
    }

    if(digitalRead(UP) == HIGH && ejecutar == 0){ //Navegar por el menú(a la derecha). No puedo ir cambiando de programas en ejecución
      delay(200);
      if(menuActual == 3){
        menuActual = 0;
      }else
        menuActual ++;
      cambio = 1;
    }

    if(digitalRead(DOWN) == HIGH && ejecutar == 0){ //Navegar por el menú(a la izquierda). No puedo ir cambiando de programas en ejecución
      delay(200);
      if(menuActual == 0){
        menuActual = 3;
      }else
        menuActual --;
      cambio = 1;
    }

    if(digitalRead(FORWARD) == HIGH){ //Ejecutar si se acciona el cuarto botón
      cambio = 1; 
      ejecutar = 1; 
    }

    if(cambio == 1 && ejecutar == 0){ //Si solo se cambia de pantalla (sin ejecutar)
      display.clearDisplay();
      menuAnterior = menuActual;
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.print(menu[menuActual]);
      display.display();
    }
  }
}

void comprobarAlarma(){ //Aparece una pantalla durante 4 segundos indicando que la alarma ha sido activada
  if(millis() >= alarmActivation){ //Se comprueba si ha pasado el tiempo establecido para la alarma
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.println("ALARMA");
    display.setCursor(0,20);
    display.setTextSize(2);
    display.print("ACTIVADA");
    display.display();
    alarmActivation = 0;
    delay(4000);
  }
}

void reloj(){ //Establecer fecha y hora actual
  DateTime now = rtc.now();
  second = now.second();
  minute = now.minute();
  hour = now.hour();
  day = now.day();
  month = now.month();
  year = now.year();

  display.clearDisplay(); //Hay que ir borrando

  // display Time
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Hora actual: ");
  display.setTextSize(1.5);
  display.setCursor(0, 10);
  display.print(hour);
  display.print(" : ");
  display.print(minute);
  display.print(" : ");
  display.print(second);

  // display date
  display.setTextSize(1);
  display.setCursor(0,30);
  display.print("Fecha actual: ");
  display.setTextSize(1.5);
  display.setCursor(0, 45);
  display.print(days[now.dayOfTheWeek()]);
  display.print(", ");
  display.print(day);
  display.print("/");
  display.print(month);
  display.print("/");
  display.print(year);

  display.display(); //Actualiza pantalla con info pasada
}

void alarma(){ //Establece alarma
  delay(150); //Delay para que de tiempo al usuario a introducir valores
  DateTime now = rtc.now();
  second = now.second();
  minute = now.minute();
  hour = now.hour();
  day = now.day();
  month = now.month();
  year = now.year();

  if (comienzoConfAlarma == 0){
    horaAlarma = hour;
    minutoAlarma = minute;
    segundoAlarma = second;
    comienzoConfAlarma = 1;
    opcionConf = 0; 
  }

  else{
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("Configurar alarma: ");

    switch(opcionConf){
      case 0:
        if(digitalRead(UP) == HIGH){ //Aumentar hora
          horaAlarma ++;
          if (horaAlarma >= 24) horaAlarma = 0;

        }
        if(digitalRead(DOWN) == HIGH){ //Disminuir hora
          horaAlarma --;
          if (horaAlarma < 0) horaAlarma = 23;
        }
        if(digitalRead(FORWARD) == HIGH){ //Aceptar hora
          opcionConf = 1;
        }
        
        if(digitalRead(BACK) == HIGH){ //Salir
          comienzoConfAlarma = 0;
          alarmaBloqueo = 0;
          ejecutar = 0; //Nos aseguramos de que no entre a ejecutar otra vez el código alarm en el loop
        }
        break;
      case 1:  
        if(digitalRead(UP) == HIGH){ //Aumentar minuto
          minutoAlarma ++;
          if (minutoAlarma >= 60) minutoAlarma = 0;
        }
        if(digitalRead(DOWN) == HIGH){ //Disminuir minuto
          minutoAlarma --;
          if (minutoAlarma < 0) minutoAlarma = 59;
        }
        if(digitalRead(FORWARD) == HIGH) //Aceptar minuto
          opcionConf = 2;
        
        if(digitalRead(BACK) == HIGH){ //Volvemos a la hora
          opcionConf = 0; 
        }
        break;
      case 2:
        if(digitalRead(UP) == HIGH){ //Aumentar segundo
          segundoAlarma ++;
          if (segundoAlarma >= 60) segundoAlarma = 0;
        }
        if(digitalRead(DOWN) == HIGH){ //Disminuir segundo
          segundoAlarma --;
          if (segundoAlarma < 0) segundoAlarma = 59;
        }
        if(digitalRead(FORWARD) == HIGH) //Aceptar segundo
          opcionConf = 3;
        
        if(digitalRead(BACK) == HIGH){ //Volvemos a minuto
          opcionConf = 1;
        }
        break;
      case 3:
        if(digitalRead(FORWARD) == HIGH) //Aceptar alarma
          opcionConf = 0;
          alarmaBloqueo = 0; //Se elimina el bloqueo, es decir se establece la alarma y se permite al usuario volver al menú de selección de aplicación
          ejecutar = 0;
        
        if(digitalRead(BACK) == HIGH){
          opcionConf = 2; //Se vuelve a segundo
        }
        break;
    }
    //PRINT
    display.setCursor(0, 10);
    display.print("Hora: ");
    display.println(horaAlarma);
    if(opcionConf > 0){ //Minutos o posterior
      display.setCursor(0, 20);
      display.print("Minuto: ");
      display.println(minutoAlarma);
    }if(opcionConf > 1){
      display.setCursor(0, 30);
      display.print("Segundo: ");
      display.println(segundoAlarma);
    }if(opcionConf > 2){
      display.setCursor(0, 40);
      display.print("Alarma establecida a las: ");
      display.print(horaAlarma);
      display.print(":");
      display.print(minutoAlarma);
      display.print(":");
      display.print(segundoAlarma);
      alarmActivation = millis() + ((horaAlarma - hour) * 3600 * 1000) + ((minutoAlarma - minute) * 60 * 1000) + ((segundoAlarma - second) * 1000); //Tiempo en milisegundos, para comparar con millis
    }
    if(alarmaBloqueo == 0){
      delay(3000); //Para tardar en cerrar el programa
    } 
    display.display();
  }
}

void temperatura(){ //Aplicación medición temperatura y humedad
  delay(300);

  //read temperature and humidity
  float t = dht.readTemperature(); //Valor temperatura
  float h = dht.readHumidity(); //Valor humedad
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  }

  display.clearDisplay();
  
  // print temperatura
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperatura: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(t);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // print humedad
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humedad: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(h);
  display.print(" %"); 
  display.display(); //Actualiza pantalla con info pasada
}
void oximetro(){//Contador pulsaciones por minuto y oxígeno en sangre
    if(iniciar_oxi == 0){
      pox.begin(); //Cada vez que se ejecuta la aplicación, se inicia el oxímetro(solo 1 vez)
      iniciar_oxi=1;
    }
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) { //Cada segundo se actualiza el valor de pulsaciones por minuto y oxígeno en sangre
      display.clearDisplay();
      display.setTextSize(1.5);
      display.setTextColor(WHITE);
      display.setCursor(0,10);
      display.println("Pulsaciones por");
      display.print("minuto: ");
      display.print(pox.getHeartRate());
      display.print(" bpm");

      display.setCursor(0, 40);
      display.println("Oxigeno en sangre:");
      display.print(pox.getSpO2());
      display.print("%");
      display.display();

      tsLastReport = millis(); //Actualiza valor
    }
}

void setup() {
  Serial.begin(115200);
  dht.begin(); //Inicializa sensor temperatura

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { //Inicializa display
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  //-----------------rtc-----------------
  if (! rtc.begin()) { //Inicializa rtc
    Serial.println("Could not find RTC! Check circuit.");
    while (1);
  }
  rtc.adjust(DateTime(__DATE__, __TIME__));
  //-----------------Oximeter-----------------
  if (!pox.begin()) { //Inicializa oxímetro
      Serial.println("FAILED");
      for(;;);
  } else {
      Serial.println("SUCCESS");
  }

  //-----------------Buttons-----------------
  pinMode(BACK, INPUT); //Asignación de los 4 botones como inputs
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(FORWARD, INPUT);

  //-----------------Display-----------------
  display.clearDisplay(); //Se elimina la imagen de arranque del OLED
  display.setTextColor(WHITE); //Color de letra blanco

  menuCreacion(); //Se crea el menú 

  display.setTextSize(1); //Define el tamaño del texto
  display.setCursor(0, 30); //Establece la ubicación en pantalla donde escribir un texto. Como actúa como cursor, el texto se irá moviendo a la derecha (e incluso haciendo salto de linea) en lugar de quedarse en una posición
  display.print(menu[menuActual]); //Se imprime la selección del menú
  display.display(); //Indica al display que tiene que mostrar todos los datos pasados hasta ahora con la opción display.print
}

void loop() { //Bucle principal
  pox.update(); //Actualizar oxímetro, se pone en esta ubicación ya que necesita actualizarse lo más rapidamente posible
  actualizarMenu();
  if(alarmActivation > 0){ //Si se ha establecido una alarma, comprobar si ha salta ya o no. (Si no hay ninguna alarmActivation == 0)
    comprobarAlarma();
  }
  if(ejecutar){ //Si se ha elegido una pantalla a ejecutar, se elige cual es el código correspondiente a la misma.
    switch(menuActual){
      case 0: reloj();
      break;
      case 1: alarmaBloqueo = 1;
        alarma();
      break;
      case 2: temperatura();
      break;
      case 3: oximetro();
      break;
    }
  }
}

