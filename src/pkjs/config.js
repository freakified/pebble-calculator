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
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];

