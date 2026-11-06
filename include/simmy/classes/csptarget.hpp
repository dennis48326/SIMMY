// @class CSPTarget after UIObjectBase UIReflow UIDialog HelpDialog
// @class CSPTarget__eTabs
// The pause menu's career submenu (game/targets_s2c.cpp CSPTarget, screen 0x23 "CSP", flow 0x1E from
// the pause menu's third tab): three tabs for the pause player's selected sim (person data from the
// person, GC vtable 0x13C):
//  - career: the career's name and icon, the job (level "n. name", male or female name), its salary,
//    hours (12 or 24 hour clock as the HUD's "time" string has it) and description, and the next level's
//    promotion requirements: friends and up to three skills with the sim's current points and the
//    needed ones (icons per skill); "Unemployed" without a career (Careers, quickdata 0x4F40C4EC);
//  - skills: the seven skill levels (points / 100) with their names (want category 3);
//  - personality: the star sign (from the five personality traits) with its icon and the five traits
//    (0..10) with their names and opposites.
// Help ("career_help", "skill_help", "personality_help") by tab.
#pragma once

struct CSPTarget__eTabs {
  SIMMY_ENUM_BODY(CSPTarget__eTabs)
};

// 0x1BC bytes.
struct CSPTarget : UIObjectBase {
  CSPTarget(CSPTarget__eTabs tab, int player);  // 801A7BC8
  ~CSPTarget() override;  // 801A8A74
  void SetVariable(char* name, char* value) override;  // 801A8C70
  char* GetVariable(char* name) override;  // 801A8DC8
  unsigned short* GetLocalizable(char* name) override;  // 801A9064
  void Update() override;  // 801A9918
  void ChangeSelectedTab(CSPTarget__eTabs tab);  // 801A991C
  void SetupCareer(cXPerson* person);  // 801A9A64
  void InstallPromotionShaders();  // 801AA0A0
  void SetupSkills(cXPerson* person);  // 801AA208
  void SetupPersonality(cXPerson* person);  // 801AA38C
  void onHelpDialogSelection(int);  // 801AA57C

  int m_exists = 0;                      // +0x88
  int m_player = 0;                      // +0x8C
  int m_tab = -1;                        // +0x90 0 career, 1 skills, 2 personality
  unsigned int m_filters[2] = {};        // +0x94 pushAptButtonFilterALL per player
  std::u16string m_description;          // +0x9C the job's description
  std::u16string m_careerName;           // +0xA0
  std::u16string m_jobName;              // +0xA4 empty: unemployed
  std::u16string m_unused;               // +0xA8
  std::u16string m_hours;                // +0xAC
  int m_salary = -1;                     // +0xB0 -1: none
  int m_reqSkill[3] = {-1, -1, -1};      // +0xB4 the next level's required skills (1..7), -1 none
  int m_reqNext[3] = {};                 // +0xC0 the points needed (/100)
  int m_reqCurr[3] = {};                 // +0xCC the sim's points
  int m_friends = 0;                     // +0xD8 the sim's friends
  int m_friendsNext = 0;                 // +0xDC the friends needed
  int m_skills[7] = {};                  // +0xE0 skill points / 100
  int m_zodiacValues[5] = {};            // +0xFC the traits (0..10)
  int m_zodiac = 0;                      // +0x110 the star sign (1..12)
  std::u16string m_zodiacName;           // +0x114
  HelpDialog m_help;                     // +0x118 (its +0x88, CSP +0x1A0: the help is up)
  UIReflow m_reflow;                     // +0x1A8
};
