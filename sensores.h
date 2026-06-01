#ifndef SENSORES_H
#define SENSORES_H

void initSensores();
void leerSensores();

extern volatile float temperaturaC;
extern volatile float humedad;

#endif