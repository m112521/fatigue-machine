# Fatigue strength testing machine

TBD:
- [ ] start button/tumbler;
- [ ] better/faster stop mechanism;
- [ ] replace wire hell with custom PCB shield;
- [ ] bigger motor;
- [ ] better on-shaft fixation;


![persp](https://github.com/m112521/notsoserous-science/assets/85460283/c4cb47e4-6cf4-4d17-a4c5-280009308a05)

## Description

Machine for determining the fatigue strength of samples with different patterns (so-called "living hinge", which makes sample exert flexibility) produced using a laser machine.

Hardware:

- Amperka Iskra Neo (Leonardo);
- Amperka Motor Shield;
- Amperka Troyka Shield;
- Hall sensor (involved into counting cycles);
- Amperka Quad Display.

Features:
- autamatic cycle counting;
- automatic stop (altough with delay).

## Generating samples

Grasshopper definition was designed to make it easier to create different patterns:

![def](https://github.com/m112521/notsoserous-science/assets/85460283/a8c173b4-ee69-4f65-a47c-f07b98875a5b)


The process of generating new samples: 

![gh-sim](https://github.com/m112521/notsoserous-science/assets/85460283/0fa1caf4-b1d9-4dbc-b486-403aa12baab0)


## Testing samples

If you wait long enough it breaks (either sample or machine): 

https://github.com/m112521/notsoserous-science/assets/85460283/56accc54-1f78-4390-a13b-5404003ea0d3


Some broken hearts:

![20230629_182247](https://github.com/m112521/fatigue-machine/assets/85460283/a31a287c-b592-4211-b958-0b256adac7ae)


