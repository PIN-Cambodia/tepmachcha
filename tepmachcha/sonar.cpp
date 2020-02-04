#include "tepmachcha.h"
#include "sonar.h"

int arraysize = 25;  //quantity of values to find the median (sample size). Needs to be an odd number
//declare an array to store the samples. not necessary to zero the array values here, it just makes the code clearer
int rangevalue[] = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int modE;

int sonarRead() {
  delay(160);
  for (int i = 0; i < arraysize; i++) {
    rangevalue[i] = pulseIn(SONAR_READ, HIGH);
    Serial.print(rangevalue[i]);
    Serial.println(F("mm"));
    delay(160);
  }

  // Sort our readings (needed to get the median)
  isort(rangevalue, arraysize);
  
  modE = mode(rangevalue, arraysize);
  return modE;
}

int sonarSpread() {
  return rangevalue[arraysize-1] - rangevalue[0];
}

/*-----------Functions------------*/

// Sorting function
// sort function (Author: Bill Gentles, Nov. 12, 2010)
void isort(int *a, int n) {
  //  *a is an array pointer function
  for (int i = 1; i < n; ++i) {
    int j = a[i];
    int k;
    for (k = i - 1; (k >= 0) && (j < a[k]); k--) {
      a[k + 1] = a[k];
    }
    a[k + 1] = j;
  }
}

//Mode function, returning the mode or median.
int mode(int *x, int n) {
  int i = 0;
  int count = 0;
  int maxCount = 0;
  int mode = 0;
  int bimodal;
  int prevCount = 0;

  while (i < (n - 1)) {
    prevCount = count;
    count = 0;

    while (x[i] == x[i + 1]) {
      count++;
      i++;
    }

    if (count > prevCount & count > maxCount) {
      mode = x[i];
      maxCount = count;
      bimodal = 0;
    }

    if (count == 0) {
      i++;
    }

    if (count == maxCount) { //If the dataset has 2 or more modes.
      bimodal = 1;
    }
    if (mode == 0 || bimodal == 1) { //Return the median if there is no mode.
      mode = x[(n / 2)];
    }

    return mode;
  }
}
