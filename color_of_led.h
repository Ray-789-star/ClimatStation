// Функции назначения цвета и мигания светодиодов
// Подключение RGB LED (подсветка)
//RGB LED с общим анодом (3.3v  -  3.3v)
#define R 5
#define G 26
#define B 19


void color_of_led(int x) {
  switch (x) {

    // когда в норме один и меньше параметров
    case 0:
    case 1:
      // Красный
      analogWrite(R, 0);
      analogWrite(G, 255);
      analogWrite(B, 255);
      Serial.println("красный");
      break;

      //когда в норме два параметра
    case 2:
      // Оранжевый
      analogWrite(R, 0);
      analogWrite(G, 155);
      analogWrite(B, 255);
      Serial.println("оранжевый");
      break;

    //когда в норме три параметра
    case 3:
      // Желтый
      analogWrite(R, 0);
      analogWrite(G, 20);
      analogWrite(B, 255);
      Serial.println("желтый");
      break;

    //когда все четыре параметра в норме
    case 4:
      // Зеленый
      analogWrite(R, 255);
      analogWrite(G, 0);
      analogWrite(B, 255);
      break;

    //при начальной загрузке параллельно с появлением логотипа
    case 5:
      // Синий
      analogWrite(R, 255);
      analogWrite(G, 255);
      analogWrite(B, 0);
      break;

    //требуется зарядка батареи
    case 6:

      // Фиолетовый
      analogWrite(R, 80);
      analogWrite(G, 255);
      analogWrite(B, 0);
      break;

    //все светодиоды отключены
    case 7:
      analogWrite(R, 255);
      analogWrite(G, 255);
      analogWrite(B, 255);
      break;

    // если поступило некорректное значение
    default:
      //Serial.println("error");
      break;
  }
}