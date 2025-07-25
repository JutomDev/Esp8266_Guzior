bool led = true;
void setup() {
  // put your setup code here, to run once:
  pinMode(4,OUTPUT);
  
}

void loop() {
  delay(1000);
  digitalWrite(4,led);
  led=!led;
  // put your main code here, to run repeatedly:

}
