# ADC selection and timings

| Signal                                                                                                                    | GPIO  | ADC1           | ADC2       | ADC3       | ADC4       | ADC5       |
| ------------------------------------------------------------------------------------------------------------------------- | ----- | -------------- | ---------- | ---------- | ---------- | ---------- |
| VBAT_SW                                                                                                                   | PA0   | yes (IN1)      | yes (IN1)  | no         | no         | no         |
| 3V3_SSD (Diamond only)                                                                                                    | PA1   | yes (IN2)      | yes (IN2)  | no         | no         | no         |
| 3V3_WIFI (Diamond only)                                                                                                   | PA2   | yes (IN3)      | no         | no         | no         | no         |
| V_SCAP_HIGH (Diamond only)                                                                                                | PB11  | yes (IN14)     | yes (IN14) | no         | no         | no         |
| HW_VERSION (Pearl) / V_SCAP_LOW (Diamond)                                                                                 | PB12  | yes (IN11)     | no         | no         | yes (IN3)  | no         |
| PVCC                                                                                                                      | PC1   | yes (IN7)      | yes (IN7)  | no         | no         | no         |
| 12V                                                                                                                       | PC2   | yes (IN8)      | yes (IN8)  | no         | no         | no         |
| 12V_CAPS                                                                                                                  | PC3   | yes (IN9)      | yes (IN9)  | no         | no         | no         |
| 3V6 (Diamond only)                                                                                                        | PD8   | no             | no         | no         | yes (IN12) | yes (IN12) |
| 3V3_SSD (EV5) / 3V8 (EV1…EV4) / 3V8_LTE (Diamond)                                                                         | PD9   | no             | no         | no         | yes (IN13) | yes (IN13) |
| 1V8                                                                                                                       | PD10  | no             | no         | yes (IN7)  | yes (IN7)  | yes (IN7)  |
| 3V3                                                                                                                       | PD11  | no             | no         | yes (IN8)  | yes (IN8)  | yes (IN8)  |
| 5V                                                                                                                        | PD12  | no             | no         | yes (IN9)  | yes (IN9)  | yes (IN9)  |
| INA240_REF                                                                                                                | PD13  | no             | no         | yes (IN10) | yes (IN10) | yes (IN10) |
| INA240_SIG                                                                                                                | PD14  | no             | no         | yes (IN11) | yes (IN11) | yes (IN11) |
| 1V2 (Diamond only)                                                                                                        | PE10  | no             | no         | yes (IN14) | yes (IN14) | yes (IN14) |
| 2V8 (Diamond only)                                                                                                        | PE11  | no             | no         | yes (IN15) | yes (IN15) | yes (IN15) |
| 1V8_SEC (Diamond only)                                                                                                    | PE14  | no             | no         | no         | yes (IN1)  | no         |
| 4V7_SEC (Diamond only)                                                                                                    | PE15  | no             | no         | no         | yes (IN2)  | no         |
| die temperature                                                                                                           | \-    | yes (IN16)     | no         | no         | no         | yes (IN4)  |
| VREFINT                                                                                                                   | \-    | yes (IN18)     | no         | yes (IN18) | yes (IN18) | yes (IN18) |
| VBAT / 3                                                                                                                  | \-    | yes (IN17)     | no         | yes (IN17) | no         | yes (IN17) |
|                                                                                                                           |       |                |            |            |            |            |
|                                                                                                                           | Pearl | Diamond (ADC1) |            |            |            |            |
| ADC clock [MHz]                                                                                                           | 42.5  | 42.5           |            |            |            |            |
| 48 tick acq channels                                                                                                      | 6     | 8              |            |            |            |            |
| 248 tick acq channels                                                                                                     | 2     | 2              |            |            |            |            |
| Oversampling factor                                                                                                       | 32    | 32             |            |            |            |            |
| Conversion time in µs for one sequence<br/>(=1/(ADC clock) _ sum_over_all_channels(acq ticks + 12) _ oversampling factor) | 662.6 | 752.9          |            |            |            |            |
