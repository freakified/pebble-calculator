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
        "label": "RPN Mode",
        "defaultValue": false,
        "description": "Enable Reverse Polish Notation (RPN) mode with a 4-register stack (T/Z/Y/X). When off, the calculator uses standard infix notation."
      },
      {
        "type": "toggle",
        "messageKey": "HAPTIC_FEEDBACK",
        "label": "Haptic Feedback",
        "defaultValue": true,
        "description": "Provide a short vibration pulse when a button is pressed."
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
