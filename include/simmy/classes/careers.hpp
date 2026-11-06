// @class Careers after ERQuickdata Career
// @class CareersImpl after Careers
// The careers (game/careers.cpp, Globs::pCareers): the "Career" table of quickdata 0x4F40C4EC. A career
// row (Career, 0x14 bytes): +0x00 its id (person data 0x38), +0x04 its ten jobs (Job, 0x70 bytes each,
// count at [-4]), +0x08 its name, +0x0C/+0x10 two more strings. A job: +0x00 ten promotion requirements
// (friends, then skill points x100 in the skill order of person data 9..0x12), +0x44 the daily salary,
// +0x48/+0x4C the start/end hour, +0x58/+0x5C the job name (male/female), +0x60/+0x64 the short name
// (male/female), +0x68 the suit, +0x6C the description. Rows are read in place (ERQuickdata accessors);
// Career and Job stand for the rows.
#pragma once

struct Careers {
  Careers();  // 800B1B54
  virtual ~Careers();  // 800B13EC
  static Careers* CreateInstance();  // 800B1420
  static void DestroyInstance(Careers* careers);  // 800B1460
};

// 0x20 bytes.
struct CareersImpl : Careers {
  CareersImpl();  // 800B1B68
  ~CareersImpl() override;  // 800B1B7C
  virtual void Load();  // 800B1500
  virtual void TearDown();  // 800B15F8
  virtual const Career* GetCareerByID(int id);  // 800B1644
  virtual int GetNumCareers();  // 800B1698
  virtual const Career* GetCareerByIndex(int index);  // 800B16A4
  virtual int GetIndexByCareer(const Career* career);  // 800B16B8
  // The "Performance" string for a job performance (-100..100: five bands at -60/-30/30/60), the
  // empty string when the set is shorter.
  virtual const unsigned short* const* GetJobPerformance(int performance);  // 800B16D8
  // The "Grades" string `grade` (or the empty string).
  virtual const unsigned short* const* GetJobGrade(int grade);  // 800B179C
  // The offer text: the career's +0x0C string followed by job `level`'s name (the female one: +0x5C).
  virtual void GetOfferDialogText(unsigned short* out, const Career& career, int level, bool female);  // 800B17D0
  // Scope 0x21: field `data` of `career`'s job `level` (0 the job count, 1 salary, 2..0xB the
  // promotion requirements, 0xC/0xD start/end hour, ...). False: no such level or field.
  virtual int GetBehCareerData(const Career* career, int level, int data, short* out);  // 800B1944
  // The job's name slot (+0x58, or the female one at +0x5C when it is not empty).
  virtual const unsigned short* const* GetJobName(const Job* job, bool female);  // 800B1AB0
  virtual const unsigned short* const* GetShortName(const Job* job, bool female);  // 800B1AE8
  virtual int GetCarpoolHour(const Job* job);  // 800B1B20
  virtual const void* GetSuit(const Job* job, bool female);  // 800B1B4C

  ERQuickdata* m_qd = nullptr;     // +0x04
  const void* m_careers = nullptr; // +0x08 the "Career" table
  const void* m_grades = nullptr;  // +0x0C WStringSet "Grades"
  const void* m_performance = nullptr;  // +0x10 WStringSet "Performance"
  const unsigned short* m_emptyText = nullptr;  // +0x1C the empty string (GC: a BString2)
};

extern Careers* Globs_pCareers;  // Globs::pCareers

// PC accessors for the rows.
namespace career {
inline int Id(const Career* c) { return ERQuickdata::S32(c, 0); }
inline const unsigned char* Jobs(const Career* c) { return static_cast<const unsigned char*>(ERQuickdata::Ptr(c, 4)); }
inline int JobCount(const Career* c) {
  const unsigned char* j = Jobs(c);
  return j ? static_cast<int>(ERQuickdata::U32(j - 4, 0)) : 0;
}
inline const Job* JobAt(const Career* c, int level) {
  return reinterpret_cast<const Job*>(Jobs(c) + level * 0x70);
}
inline const unsigned short* const* Name(const Career* c) { return ERQuickdata::LocSlot(c, 8); }
inline int Requirement(const Job* j, int i) { return ERQuickdata::S32(j, i * 4); }
inline int Salary(const Job* j) { return ERQuickdata::S32(j, 0x44); }
inline int StartHour(const Job* j) { return ERQuickdata::S32(j, 0x48); }
inline int EndHour(const Job* j) { return ERQuickdata::S32(j, 0x4c); }
inline const unsigned short* const* Description(const Job* j) { return ERQuickdata::LocSlot(j, 0x6c); }
}  // namespace career
