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
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
