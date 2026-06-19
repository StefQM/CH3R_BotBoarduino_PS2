# LED Lights Control

This document explains how to control the robot's LED patterns using the PS2 remote controller and the serial API.

## Activating LED Patterns via Remote

The robot features a dedicated, highly interactive **LED Mode** that allows you to configure and tweak the LED patterns in real-time using the PS2 controller. While in LED Mode, the robot's physical movement is disabled to keep it completely stationary and safe during configuration.

### Entering & Exiting LED Mode

-   **L1 Button (Long Press, >0.5 seconds):** Toggles LED Mode on or off.
    -   *Entering* LED Mode plays a rising 3-tone beep confirmation.
    -   *Exiting* LED Mode plays a falling 3-tone beep confirmation.
-   **L1 Button (Short Press, <=0.5 seconds):** Retains its standard functionality (toggles **Translate Mode**).

---

### LED Mode Control Scheme

Once **LED Mode** is enabled, the controller inputs are mapped as follows:

| Control | Action | Target / Function |
| :--- | :--- | :--- |
| **Left Joystick (L/R)** | Cycle Ring Patterns | Switches the current animation pattern of the Ring |
| **Left Joystick (U/D)** | Cycle Ring Colors | Loops through a predefined set of 10 vibrant colors for the Ring |
| **Right Joystick (L/R)** | Cycle Leg Patterns | Switches the current animation pattern of the Leg LEDs |
| **Right Joystick (U/D)** | Cycle Leg Colors | Loops through a predefined set of 10 vibrant colors for the Leg LEDs |
| **D-Pad (U/D)** | Speed Up / Down | Adjusts the animation speed of **both** Ring & Leg patterns |
| **D-Pad (L/R)** | Modify Custom Attribute | Tweaks a pattern-specific attribute on **both** Ring & Leg patterns |

### Custom Attributes (D-Pad L/R)

The D-Pad Left/Right inputs control pattern-specific attributes. For example:
-   **Meteor Tail (`RING_METEORTAIL`):** Dynamically adjusts the length of the trailing fade (meteor tail).
    -   *D-Pad Right* lengthens the tail (slower fade/longer trailing tail).
    -   *D-Pad Left* shortens the tail (faster fade/shorter trailing tail).

---

### Predefined Colors

The color cycle loops through the following 10 vibrant, high-intensity presets:
1. White (Default)
2. Red
3. Green
4. Blue
5. Yellow
6. Magenta
7. Cyan
8. Orange
9. Purple
10. Pink

---

## Serial API Protocol

For more advanced control, you can use the serial interface. Connect to the robot's serial port to send commands.

### Command Structure

All serial commands must start with a `#` character and end with an `!` character.

The general format is: `#{TYPE}{INDEX}_{ACTION}_{KEY=VALUE}!`

-   **TYPE:** `R` for the main ring, `L` for the leg LEDs.
-   **INDEX:** (Only for Legs) `0` for all legs, or `1`-`6` for a specific leg.
-   **ACTION:** The numerical ID of the pattern to activate (see tables below).
-   **KEY=VALUE:** Optional parameters to control the pattern.
    -   `C=HEXCOLOR`: Sets the color using a hexadecimal value (e.g., `FF0000` for red).
    -   `B=BRIGHTNESS`: Sets the brightness from `0` to `255`.
    -   `S=SPEED`: Sets the animation speed from `0` to `255`.
    -   `A=ATTRIBUTE`: Sets custom pattern attributes from `0` to `255` (e.g., Meteor tail length).

### Virtual Inputs API

You can simulate physical leg contacts (foot sensors) via the serial API for testing:

-   `#V0_1!`: Enable Virtual Leg Sensors mode (robot ignores physical microswitches, listens to serial).
-   `#V0_0!`: Disable Virtual Leg Sensors mode (robot reads physical MCP23017 switches).
-   `#V{1-6}_{1/0}!`: Simulates foot sensor press (`1`) or release (`0`) for the specified leg index (1 to 6). For example, `#V1_1!` presses the Leg 1 switch.

### Query Command

To get the current state of the LEDs, send the query command:

`#?!`

This will return a JSON string with the current ring and leg patterns, colors, and brightness.

### Example Commands

-   `#R0_4_S=50!`: Set the main ring to the Rainbow pattern with a speed of 50.
-   `#L1_1_C=00FF00!`: Set the LED for Leg 1 to a solid green color.
-   `#L0_6!`: Set all leg LEDs to the interactive Contact mode.

## LED Patterns

The following tables list the available patterns and their corresponding numerical IDs for the serial API.

### Ring Patterns

| ID | Pattern Name      | Description                                           |
|----|-------------------|-------------------------------------------------------|
| 0  | `RING_OFF`        | Turns all ring LEDs off.                              |
| 1  | `RING_COLOR`      | Displays a solid color.                               |
| 2  | `RING_COLOR_FADING`| **One-Shot Fade:** Blends the requested color directly down to Black/OFF over time (no repeat). |
| 3  | `RING_METEORTAIL` | A meteor/comet that wipes around the ring with a fading tail. |
| 4  | `RING_RAINBOW`    | A dynamic rainbow effect that cycles around the ring. |
| 5  | `RING_BREATH`     | A gentle breathing/pulsing effect for the whole ring. |

### Leg Patterns

| ID | Pattern Name             | Description                                                                 |
|----|--------------------------|-----------------------------------------------------------------------------|
| 0  | `LEG_OFF`                | Turns the leg LED off.                                                      |
| 1  | `LEG_COLOR`              | Displays a solid color.                                                     |
| 2  | `LEG_COLOR_FADING`       | **One-Shot Fade:** Blends the requested color directly down to Black/OFF over time (no repeat). |
| 3  | `LEG_COLOR_TOUCH`        | Shows the color only when the leg's foot sensor is pressed.                 |
| 4  | `LEG_COLOR_TOUCH_FADING` | **Fades Out on Press:** The color appears on touch and immediately starts fading out to Black over time (releasing has no action). |
| 5  | `LEG_COLOR_FROM_RING`    | The leg LED mirrors the color of the adjacent section of the main ring.     |
| 6  | `LEG_CONTACT`            | **Interactive Mode:** Shows a dynamic expanding wave of the leg's defined Color on the ring when lifted. |
| 7  | `LEG_CONTACT_FADING`     | **Inward-Shrinking Wave:** Lifting a foot triggers a wave of the leg's defined Color that dynamically shrinks inwards back to the center while fading into the active background ring pattern. |
