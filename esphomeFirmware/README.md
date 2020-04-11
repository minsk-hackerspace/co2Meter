ESPHome
-------

This firmware is for esphome project, integrating nicely with HomeAssistant.

```sh
vi co2meter.yaml # edit your passwords and credentials
esphome co2meter.yaml run
```

Known issues:
- displayed info is a bit different, no ppm charts out of the box
- dht data pin is rewired to D6 on the pcb, was not usable on pin 9

