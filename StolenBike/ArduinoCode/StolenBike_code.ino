/*
El funcionamiento de este programa es el siguiente:
  -En cualquier momento, si realizamos una llamada al número de la tarjeta introducida en el aparato, este nos responderá con un SMS que contenga las coordenadas
  de la ubicación actualizadas. El SMS se enviará en el formato correcto para que en el móvil sólo sea pulsar el enlace del SMS y abrir google maps. 
  -Hasta que no activemos la función LOCKED, no hará nada más que lo anteriormente mencionado. 
  -Una vez que activemos la función LOCKED, el dispositivo almacenará las coordenadas para avisarnos en caso de que estas cambien (es decir, si la bici se mueve).
  -Además de avisarnos si la bici se mueve cuando debería de estar candada, en caso de que reciba una sacudida fuerte, el acelerómetro lo detecta y nos manda un SMS. 
*/

//HERRAMIENTAS QUE NECESITA EL PROGRAMA
#include <SoftwareSerial.h> //Para la comunicación serie en un pin I/O digital.
#include <Adafruit_GPS.h>   //Librería necesaria para el GPS de Adafruit.
#include <AltSoftSerial.h>  //Como SoftwareSerial sólo nos permite crear un cliente, utilizamos otra librería para utilizar otros dos pines para la comunicación serie. 
#define enciendeModulo 2    //Es el pin que hemos conectado al módulo de la SIM900 para encenderlo mediante hardware. 

SoftwareSerial mySerial(6,7); //Establece el cliente de la librería SoftwareSerial en los pines 6(RX) y 7(TX).
AltSoftSerial BT1;            //En AltSoftSerial no podemos definir nosotros los pines, por defecto son: 8(RX) y 9(TX).
Adafruit_GPS GPS(&mySerial);  //Creamos el cliente del GPS de Adafruit denominado "GPS" en los pines 6(RX) y 7(TX).

//Variables SIM900
boolean SMSenviado = false;   //Esta variable es la que almacena la información de si el SMS ha sido enviado o no.
char number[20];              //Este array almacena el número de la llamada entrante en forma bruta, tal y como nos lo da el comando AT de la SIM900.
char realnumber[9];           //En este array almacenamos el número limpio (sin caracteres inútiles) del teléfono que llama. 
char mynumber[9];             //En este array guardaremos nuestro propio número para así poderlo comparar con el número de la llamada entrante. 
int a = 0;                    //Es una variable que utilizamos como herramienta para comparar el número de la llamada entrante con el nuestro, como veremos más adelante. 
bool llamada = false;


//Variables HC-06
bool locked;                  //Esta variable almacenará el estado de LOCKED o NO LOCKED.
bool reset;                   //Esta variable almacenará el estado de la función reset. 
char entrada;                 //Esta variable almacenará el caracter que enviemos desde el bluetooth.

//MPU6050 variables y declaraciones
boolean alerta = false;       //Esta variable almacenará si el movimiento de la bici ha superado al movimiento programado, o no. 
#include "I2Cdev.h"           //Incluímos librerías necesarias tanto como para la comunicación I2C como para el funcionamiento del MPU6050.
#include "MPU6050.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif
MPU6050 accelgyro;            //Creamos un cliente denominado 'accelgyro'.
int16_t ax, ay, az;           //Se almacenan valores del acelerómetro (ejes x,y,z).
int16_t gx, gy, gz;           //Se almacenan valores del giroscopio (ejes x,y,z).
#define OUTPUT_READABLE_ACCELGYRO   //Manera en la que se van a mandar los datos de todos los ejes, en este caso: ACCEL X,Y,Z -- GYRO X,Y,Z
int ejex = 0;                 //En estas variables almacenamos el valor del eje X, Y, Z.
int ejey = 0;
int ejez = 0;


//GPS VARIABLES Y DECLARACIONES
String NMEA1;                 //Variable para la primera sentencia NMEA
String NMEA2;                 //Variable para la segunda sentencia NMEA
char c;                       //Para leer caracteres que provienen del GPS. 
float deg;                    //Almacenará por un breve periodo de tiempo la latitud o longitud en un formato simple. 
float degWhole;               //Variable para la parte entera de la posición.
float degDec;                 //Variable para la parte decimal de la posición.
float Latitud;                //Variable donde almacenaremos la latitud.
float Longitud;               //Variable donde almacenaremos la longitud.
bool saveGPS = 0;             //Al activar la función LOCKED, esta variable actuará de "bandera" para guardar la posición sólo una vez.
int saveGPSLatitud;           //La latitud de la posición guardada se almacenará en esta variable.
int saveGPSLongitud;          //La longitud de la posición guardada se almacenará en esta variable.
int refreshGPSLatitud;        //Mientras ya tenemos la posición guardada, en esta variable seguiremos almacenando la latitud y comparándola con la guardada.
int refreshGPSLongitud;       //Mientras ya tenemos la posición guardada, en esta variable seguiremos almacenando la longitud y comparándola con la ya guardada.



//FUNCIÓN PARA MANDAR COMANDOS AT A LA SIM900, el formato será el siguiente:
//ATSIM(COMANDO A ENVIAR , RESPUESTA ESPERADA , TIEMPO LÍMITE).
int8_t ATSIM(char* ATcommand, char* expected_answer, unsigned int timeout){
    uint8_t x=0,  answer=0;   //Son las variables que utilizaremos en esta función. 'x' será el contador del array response, y answer la respuesta que dará la función (true/false).
    char response[100];       //Este array almacena la respuesta que obtenemos del módulo SIM900.
    unsigned long previous;   //Almacena el valor inicial de millis() para poder compararlo luego con el timeout. 

    memset(response, '\0', 100);                     // Inicia el array.
    delay(100);
    
    while( Serial.available() > 0) Serial.read();    // Limpia el buffer de entrada
    Serial.println(ATcommand);                       // Manda el comando AT

    x = 0;
    previous = millis();           //A la variable previous le asigna el valor millis(). millis() seguirá corriendo y así podremos compararlo con previous para que no exceda el timeout.

    //Este loop espera a la respuesta del módulo
    do{
        if(Serial.available() != 0){    
            response[x] = Serial.read();  //Cuando haya respuesta del módulo, almacena la misma en el array 'response[]'.
            x++;                          //Suma el contador con el objetivo de almacenar todos los datos entrantes.
            if (strstr(response, expected_answer) != NULL)    //Mediante la función strstr(), compara la variable response con la respuesta esperada.
            {
                answer = 1;               //Si la respuesta es igual a la respuesta esperada, da a la variable answer el valor '1'.
            }
        }
    }while((answer == 0) && ((millis() - previous) < timeout));   //Realiza la función hasta que la respuesta sea la esperada o hasta que transcurra el timeout estabelcido.
    return answer;  //Devuelve la respuesta a la función. 
}



//FUNCION CONECTAR SIM900
void conectar()  {
  if (ATSIM("AT","OK",2000) == 1)  {                        //Comprueba que esté encendida la SIM. AT - OK.
      ATSIM("AT+CPIN=XXXX","OK",XXXX);                      //Si está encendido el módulo, introduce el pin. 
      if(ATSIM("AT+CREG?","+CREG: 0,0",5000) == 0)  {       //Comprueba si tiene cobertura.
        BT1.println(F("Conectado a la red"));
        delay(1000);
      } else {
        BT1.println(F("Error al conectar"));
      }
  } else  {
      while (ATSIM("AT","OK",2000) == 0) {                  //Si el módulo está apagado, enciéndelo.
        digitalWrite(enciendeModulo,HIGH);
        delay(1000);
        digitalWrite(enciendeModulo,LOW);
        delay(1000);
      }
      conectar();                                           //Tras encenderlo, inicia otra vez la función.
  }
  
  //Establece un número de teléfono 
  mynumber[0]='X';
  mynumber[1]='X';
  mynumber[2]='X';
  mynumber[3]='X';
  mynumber[4]='X';
  mynumber[5]='X';
  mynumber[6]='X';
  mynumber[7]='X';
  mynumber[8]='X';
  delay(50);
} 


//FUNCIÓN PARA MANDAR SMS
void SMScoordenadas()  {
  if(ATSIM("AT+CMGF=1","OK",3000) == 1)  {                    //Establece el modo SMS del módulo SIM900. 
    if (ATSIM("AT+CMGS=\"XXXXXXXXX\"",">",2000) == 1) {       //Introduce el número de teléfono al que queremos enviar el SMS. 
      coordenadas();                                          //Llamamos a la función 'coordenadas' y mandamos un SMS estructurado con el enlace a google maps. 
      Serial.print(F("https://maps.google.com/?q="));
      Serial.print(Latitud,5);
      Serial.print(F(","));
      Serial.print(Longitud,5);
      Serial.write(0x1A);
    }
  } else  {                                                   //Si ha habido un error al establecer el modo SMS, vuelve a intentarlo hasta que sea capaz de mandarlo. 
    SMScoordenadas();
  }
}

void SMSacelerometro()  {
  if(ATSIM("AT+CMGF=1","OK",3000) == 1)  {                    //Establece el modo SMS del módulo SIM900. 
    if (ATSIM("AT+CMGS=\"XXXXXXXXX\"",">",2000) == 1) {       //Introduce el número de teléfono al que queremos enviar el SMS. 
      Serial.print(F("La bici ha recibido una sacudida."));
      Serial.write(0x1A);
    }
  } else  {                                                   //Si ha habido un error al establecer el modo SMS, vuelve a intentarlo hasta que sea capaz de mandarlo. 
    SMSacelerometro();
  }
}


//FUNCIÓN QUE INICIA MPU6050
void iniciar_acelerometro()   {
    //Se une al BUS I2C (la librería I2Cdev no lo hace automáticamente por lo que hay que hacerlo a mano).
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    accelgyro.initialize(); //Inicia el acelerómetro
    delay(1000);

    BT1.println(accelgyro.testConnection() ? F("MPU6050 conexion realizada") : F("MPU6050 fallo al conectar")); //Verifica la conexión del acelerómetro.
    delay(1000);
}


//FUNCIÓN QUE MIDE VALORES DEL MPU6050
bool medir_acelerometro() 
{
  accelgyro.getRotation(&gx, &gy, &gz);   //Obtenemos los valores del giroscopio, ejes X,Y,Z.
  if ( gx > 15000 || gx < -15000 || gy > 15000 || gy < -15000 || gz > 15000 || gz < -15000)  {   //Los comparamos y en caso de que un valor del acelerómetro descuadre, devuelve true, y si no false. 
   return true;
  } else  {
   return false;
  }
}


//FUNCIÓN HC-06
void bluetooth()  {
   if (BT1.available())  {      //Si el módulo bluetooth le habla al Arduino, leemos la entrada... 
    entrada = BT1.read();
    if (entrada == 'A')   {     //Si es 'A', entramos en el modo LOCKED. 
      BT1.println (F("LOCKED"));  
      locked = true;
     }
    if (entrada == 'B')   {     //Si es 'B', salimos del modo LOCKED.
      BT1.println (F("UNLOCKED"));  
      locked = false;
      saveGPS = 0;
    }
    if (entrada == 'C')   {     //Si es 'C', salimos del modo locked, y desactivamos las variables que se modifican al usar el resto del programa. 
      BT1.println (F("RESET. La alarma esta desactivada."));  
      locked = false;
      SMSenviado = false;
      alerta = false;
      saveGPS = 0;
    }
  }
}



//FUNCIONES GPS
void coordenadas()  {
  readGPS();                                        //Lee el GPS en bruto.
  if(GPS.fix==1)  {                                 //Cuando el GPS tenga cobertura... 
    degWhole=float(int(GPS.longitude/100));         //Obtenemos la parte entera de la longitud. 
    degDec = (GPS.longitude - degWhole*100)/60;     //Obtenemos la parte decimal de la longitud. 
    deg = degWhole + degDec;                        //Estructuramos en una variable la parte entera y la parte decimal. 
  if (GPS.lon=='W') {                               //Si vives en el hemisferio oeste, con el objetivo de escribir las coordenadas con el mismo idioma que google maps, tienes que poner coordenadas negativas. 
    deg= (-1)*deg;
  }
  Longitud = deg;                                   //Guardamos la longitud bien estructurada en una variable denominada longitud. 
  degWhole=float(int(GPS.latitude/100));            //Obtenemos la parte entera de la latitud.
  degDec = (GPS.latitude - degWhole*100)/60;        //Obtenemos la parte decimal de la latitud. 
  deg = degWhole + degDec;                          //Estructuramos en una variable la parte entera y la parte decimal. 
  if (GPS.lat=='S') {                               //Si vives en el hemisferio sur, con el objetivo de escribir las coordenadas con el mismo idioma que google maps, tiene que ser negativo. 
    deg = (-1)*deg;
  }
    Latitud = deg;                                  //Guardamos la latitud bien estructurada en una variable denominada latitud. 
  }
}


void readGPS() {
  clearGPS();                         //Llama a la función clearGPS que limpia información vieja y corrupta del puerto serie. 
  while(!GPS.newNMEAreceived()) {     //Ejecuta una y otra vez hasta obtener una sentencia NMEA correcta. 
    c=GPS.read();
  }
  GPS.parse(GPS.lastNMEA());          //Analiza la sentencia NMEA. 
  NMEA1=GPS.lastNMEA();               //Almacena la sentencia en la variable NMEA1
  
   while(!GPS.newNMEAreceived()) {    //Ejecuta una y otra vez hasta obtener la sentencia NMEA correcta. 
    c=GPS.read();
  }
  GPS.parse(GPS.lastNMEA());          //Analiza la sentencia NMEA. 
  NMEA2=GPS.lastNMEA();               //Almacena la sentencia en la variable NMEA2.
}


void clearGPS() {
  while(!GPS.newNMEAreceived()) {     //Ejecuta una y otra vez hasta obtener la sentencia NMEA correcta. 
    c=GPS.read();
  }
  GPS.parse(GPS.lastNMEA());          //Analiza la sentencia NMEA. 
  while(!GPS.newNMEAreceived()) {     //Ejecuta una y otra vez hasta obtener la sentencia NMEA correcta. 
    c=GPS.read();
  }
  GPS.parse(GPS.lastNMEA());          //Analiza la sentencia NMEA.
   while(!GPS.newNMEAreceived()) {    //Ejecuta una y otra vez hasta obtener la sentencia NMEA correcta. 
    c=GPS.read();
  }
  GPS.parse(GPS.lastNMEA());          //Analiza la sentencia NMEA. 
}


void incomingcall()  {
   if(ATSIM("","+CLIP",1000) == 1) {        //Comprueba si el módulo está recibiendo una llamada. 
    for (int i=0; i<19; i++)  {             //Si estamos recibiendo una llamada, guarda el número en bruto en el array number[].
      while (Serial.available() == 0) {
        delay (50);
      }
      number[i] = Serial.read();  
    }
    Serial.flush();                         //Tras guardar el número, limpia el puerto serie. 


    for (int i=0; i<=14; i++){             //Borra caracteres innecesarios (comilla) y guarda el número en un nuevo array que contendrá el número limpio, llamado (realnumber).
      if(number[i]== '"'){
        i++;
        realnumber[0]=number[i];
        i++;
        realnumber[1]=number[i];
        i++;
        realnumber[2]=number[i];
        i++;
        realnumber[3]=number[i];
        i++;
        realnumber[4]=number[i];
        i++;
        realnumber[5]=number[i];
        i++;
        realnumber[6]=number[i];
        i++;
        realnumber[7]=number[i];
        i++;
        realnumber[8]=number[i];
        break;
      }  
    }


    for (int i=0;i<9;i++){                  //Comparamos caracter a caracter el número que ha llamado al módulo, y el número que nosotros tenemos guardado. 
     if (realnumber[i] == mynumber[i]){
       a++;                                 //Si el caracter es correcto, incrementamos el valor de la variable 'a'. 
     }
    }

    if(a==9){                               //Si 'a' tiene el valor de 9, significa que los 9 caracteres son correctos y por ende, que el número que llama es el que nosotros hemos definido.
     ATSIM("ATH", "OK", 1000);              //Colgamos la llamada y devolvemos un true a la función. 
     llamada = true;
    }else{                                  //Si el número no es correcto, colgamos la llamada y devolvemos un false a la función.
     ATSIM("ATH", "OK", 1000);
     llamada = false;
    }
  
    a = 0;                                  //Tras hacer lo que necesitamos, devolvemos a la variable 'a' el valor 0 y limpiamos el puerto serie. 
    Serial.flush();
  }
}


void setup() {
  Serial.begin(9600);                       //Iniciamos el puerto serie. 
  GPS.begin(9600);                          //Iniciamos el GPS.
  GPS.sendCommand("$PGCMD,33,0*6D");
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  BT1.begin(9600);                          //Iniciamos el bluetooth. 
  conectar();                               //Llamamos a la función conectar().
  iniciar_acelerometro();                   //Llamamos a la función iniciar_acelerometro().
}

void loop() {
  bluetooth();                              //Esperamos al bluetooth y a la vez esperamos una llamada... 
  if (locked == true)  {                    //Si activamos el modo LOCKED...
    if (saveGPS == 0)   {                   //Guardamos las coordenadas actuales.
      coordenadas();
      saveGPSLatitud = Latitud * 1000;
      saveGPSLongitud = Longitud * 1000; 
      saveGPS = 1;
    }
    coordenadas();                          //Y seguimos obteniendo coordenadas y comparándolas con las que hemos guardado. 
    refreshGPSLatitud = Latitud * 1000;
    refreshGPSLongitud = Longitud * 1000; 
    if (refreshGPSLatitud != saveGPSLatitud && SMSenviado == false || refreshGPSLongitud != saveGPSLongitud && SMSenviado == false) { //En caso de que haya variación, nos informa.
      BT1.println(F("La bici se ha movido de lugar!"));
      SMScoordenadas();
      SMSenviado = true;
    }
    if (medir_acelerometro() == true && SMSenviado == false) {    //Si el acelerómetro detecta movimiento, nos informa. 
      BT1.println(F("¡MOVIMIENTO!"));
      SMSacelerometro();
      delay(1000);
      SMSenviado = true;
    }
  }
  incomingcall();
  if (llamada == true)  {      //Si en cualquier momento recibimos una llamada, nos informa. 
    SMScoordenadas();
    llamada = false;
  }
}
