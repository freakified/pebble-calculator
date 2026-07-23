// Curated functions assignable to the physical buttons. The `value`s MUST match
// the KeyFunc enum in src/c/calc_settings.h (append-only; never reorder/reuse).
var BUTTON_ACTION_OPTIONS = [
  { "label": "None (disabled)", "value": 0 },
  { "label": "Next Page", "value": 1 },
  { "label": "Previous Page", "value": 2 },
  { "label": "Backspace", "value": 3 },
  { "label": "Clear", "value": 4 },
  { "label": "± Negate", "value": 5 },
  { "label": "Equals (=)", "value": 6 },
  { "label": "Enter (RPN push)", "value": 7 },
  { "label": "Swap X↔Y", "value": 8 },
  { "label": "Roll Down", "value": 9 },
  { "label": "Roll Up", "value": 10 },
  { "label": "Drop", "value": 11 },
  { "label": "Last X", "value": 12 },
  { "label": "Memory Recall", "value": 13 },
  { "label": "M+", "value": 14 },
  { "label": "M−", "value": 15 },
  { "label": "Memory Clear", "value": 16 },
  { "label": "DEG/RAD Toggle", "value": 17 },
  { "label": "2nd Toggle", "value": 18 },
  { "label": "π (pi)", "value": 19 },
  { "label": "e", "value": 20 }
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
        "description": "If enabled, the calculator will use RPN mode with a 4-register stack."
      },
      {
        "type": "text",
        "defaultValue": "<strong>About RPN</strong><br>RPN stands for Reverse Polish Notation, and has been around for a while, though it's mostly favored by financial analysts. A key advantage of RPN is that operations occur in an entirely consistent manner, it does not require parenthesis, and allows you to fix your mistakes without wiping out your entire calculation!<br><br><strong>Let's learn RPN!</strong><br>To add 1 and 5, you would press 1, then ENTER, then 5, then PLUS. For more exciting recipes, check out this <a href='https://hansklav.home.xs4all.nl/rpn/'>interesting tutorial</a>!"
      }
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
        "description": "If enabled, watch vibrates slightly when buttons are pressed."
      },
      {
        "type": "toggle",
        "messageKey": "KEEP_BACKLIGHT",
        "label": "Keep Backlight On",
        "defaultValue": false,
        "description": "If enabled, backlight will stay on while Calculator app is active."
      },
      {
        "type": "toggle",
        "messageKey": "SWIPE_PAGING",
        "label": "Swipe to Change Pages",
        "defaultValue": true,
        "description": "If enabled, swipe left/right on the screen to move between pages. Disable if you keep changing pages by accident."
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
        "defaultValue": "Assign a function to each physical button. The Back button always exits the app."
      },
      {
        "type": "select",
        "messageKey": "KEY_UP_ACTION",
        "label": "Top Button",
        "defaultValue": 5,
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
        "defaultValue": 3,
        "options": BUTTON_ACTION_OPTIONS
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

