var BUTTON_ACTION_OPTIONS = [
  { "label": "None (disabled)", "value": 0 },
  {
    "label": "Navigation & Editing",
    "value": [
      { "label": "Next Page", "value": 1 },
      { "label": "Previous Page", "value": 2 },
      { "label": "Backspace", "value": 3 },
      { "label": "Clear", "value": 4 }
    ]
  },
  {
    "label": "Basic Arithmetic",
    "value": [
      { "label": "+ Add", "value": 21 },
      { "label": "− Subtract", "value": 22 },
      { "label": "× Multiply", "value": 23 },
      { "label": "÷ Divide", "value": 24 }
    ]
  },
  {
    "label": "Operations",
    "value": [
      { "label": "± Negate", "value": 5 },
      { "label": "Equals (=)", "value": 6 },
      { "label": "√x Square Root", "value": 25 },
      { "label": "x² Square", "value": 26 },
      { "label": "1/x Reciprocal", "value": 27 },
      { "label": "% Percent", "value": 29 },
      { "label": "EE (exponent)", "value": 30 }
    ]
  },
  {
    "label": "Memory",
    "value": [
      { "label": "Memory Recall", "value": 13 },
      { "label": "M+", "value": 14 },
      { "label": "M−", "value": 15 },
      { "label": "Memory Clear", "value": 16 }
    ]
  },
  {
    "label": "Modes & Constants",
    "value": [
      { "label": "DEG/RAD Toggle", "value": 17 },
      { "label": "2nd Toggle", "value": 18 },
      { "label": "π (pi)", "value": 19 },
      { "label": "e", "value": 20 }
    ]
  },
  {
    "label": "RPN Only",
    "value": [
      { "label": "Enter (RPN push)", "value": 7 },
      { "label": "Swap X↔Y", "value": 8 },
      { "label": "Roll Down", "value": 9 },
      { "label": "Roll Up", "value": 10 },
      { "label": "Drop", "value": 11 },
      { "label": "Last X", "value": 12 },
      { "label": "Stack Clear", "value": 28 }
    ]
  }
];

module.exports = [
  {
    "type": "heading",
    "defaultValue": "Calculator Settings"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Mode"
      },
      {
        "type": "toggle",
        "messageKey": "RPN_MODE",
        "label": "Enable RPN Mode",
        "defaultValue": false,
        "description": "If enabled, the calculator will use RPN mode! Learn more about RPN over at this <a href='http://linuxfocus.org/~guido/hp_calc/handbooks/rpn-tutorial.html'>RPN Tutorial</a>!"
      },
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "General"
      },
      {
        "type": "toggle",
        "messageKey": "HAPTIC_FEEDBACK",
        "label": "Haptic Feedback",
        "defaultValue": true,
        "description": "If enabled, the watch will vibrate slightly when pressing calculator buttons."
      },
      {
        "type": "toggle",
        "messageKey": "KEEP_BACKLIGHT",
        "label": "Keep Backlight On",
        "defaultValue": false,
        "description": "If enabled, the backlight will stay on while the Calculator app is open."
      },
      {
        "type": "toggle",
        "messageKey": "SWIPE_PAGING",
        "label": "Swipe to Change Pages",
        "defaultValue": true,
        "description": "If enabled, you can change pages by swiping left or right on the screen."
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Buttons"
      },
      {
        "type": "text",
        "defaultValue": "Assign your favorite actions to the physical buttons!"
      },
      {
        "type": "select",
        "messageKey": "KEY_UP_ACTION",
        "label": "Top Button",
        "defaultValue": 3,
        "options": BUTTON_ACTION_OPTIONS
      },
      {
        "type": "select",
        "messageKey": "KEY_SELECT_ACTION",
        "label": "Middle Button",
        "defaultValue": 1,
        "options": BUTTON_ACTION_OPTIONS
      },
      {
        "type": "select",
        "messageKey": "KEY_DOWN_ACTION",
        "label": "Bottom Button",
        "defaultValue": 5,
        "options": BUTTON_ACTION_OPTIONS
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

