#include <iostream>
#include <vector>
#include <sstream>
#include <set>
//https://www.spoj.com/problems/HOTLINE/

struct Statement
{
  std::string sub;
  std::string pred;
  std::string obj;
  bool positive;
  Statement() :positive(true) {}
};

struct Question
{
  //do does who what
  std::string marker;
  std::string sub;
  std::string pred;
  std::string obj;
};

struct Dialogue
{
  std::vector<Statement> s;
  bool contradiction;
  Dialogue() :contradiction(false)
  {}
};

std::string ParseObj(std::istringstream& iss)
{
  std::string tok;
  std::ostringstream oss;
  while (iss >> tok) {
    if (tok.back() == '.' || tok.back() == '?') {
      tok = tok.substr(0, tok.size() - 1);
      oss << tok;
      break;
    }
    else {
      oss << tok << " ";
    }
  }
  return oss.str();
}

Statement ParseStmt(const std::string & line)
{
  Statement s;
  std::istringstream iss(line);
  iss >> s.sub;
  
  std::string tok;
  iss >> tok;
  if (tok == "don't" || tok == "doesn't") {
    s.positive = false;
    iss >> tok;
  }
  bool endSentence = false;
  if (tok.back() == '.') {
    tok = tok.substr(0, tok.size() - 1);
    endSentence = true;
  }
  s.pred = tok;
  if (!endSentence) {
    s.obj = ParseObj(iss);
  }
  //remove s from singular positive predicates
  if (s.sub != "I" && s.sub != "you" && s.positive) {
    if (s.pred.back() == 's') {
      s.pred = s.pred.substr(0, s.pred.size() - 1);
    }
  }
  return s;
}

bool contradict(const Statement& a, const Statement& b)
{
  if (a.pred != b.pred || a.obj != b.obj) {
    return false;
  }

  if (a.sub == b.sub && a.positive == b.positive) {
    return false;
  }

  bool posa = a.positive;
  bool posb = b.positive;
  if (a.sub == "nobody") {
    posa = false;
  }
  if (b.sub == "nobody") {
    posb = false;
  }

  if (posa == posb) {
    return false;
  }

  if (a.sub == "everybody") {
    if (!posb) {
      return true;
    }
  }

  if (b.sub == "everybody") {
    if (!posa) {
      return true;
    }
  }

  if (a.sub != b.sub) {
    if (a.sub == "nobody" || b.sub == "nobody") {
      return true;
    }
    return false;
  }

  return true;
}

bool contradict(const Statement& stmt, const Dialogue& d)
{
  for (size_t i = 0; i < d.s.size(); i++) {
    if (contradict(stmt, d.s[i])) {
      return true;
    }
  }
  return false;
}

void AddStmt(const std::string& str, Dialogue & d)
{
  Statement stmt = ParseStmt(str);
  if (!d.contradiction) {
    bool c = contradict(stmt, d);
    d.contradiction = c;
  }
  d.s.push_back(stmt);
}

Question ParseQuestion(const std::string& line)
{
  std::istringstream iss(line);
  Question q;
  iss >> q.marker;
  if (q.marker == "do" || q.marker == "does") {
    iss >> q.sub;
    iss >> q.pred;
    if (q.pred.back() == '?') {
      q.pred = q.pred.substr(0, q.pred.size() - 1);
    }
    else {
      q.obj = ParseObj(iss);
    }
  }
  else if (q.marker == "who") {
    iss >> q.pred;
    if (q.pred.back() == '?') {
      q.pred = q.pred.substr(0, q.pred.size() - 1);
    }
    else {
      q.obj = ParseObj(iss);
    }
    if (q.pred.back() == 's') {
      q.pred = q.pred.substr(0, q.pred.size() - 1);
    }
  }
  else if (q.marker == "what") {
    std::string tok;
    iss >> tok;
    iss >> q.sub;
    iss >> tok;
  }
  return q;
}

bool plural(const std::string& sub)
{
  return (sub == "you" || sub == "I");
}

std::string AnswerYesNo(const Question& q, const Dialogue& d) 
{
  bool positive = true;
  bool match = false;
  for (size_t i = 0; i < d.s.size(); i++) {
    const Statement & stmt = d.s[i];
    if (q.pred != stmt.pred || q.obj != stmt.obj) {
      continue;
    }

    if (q.sub == stmt.sub) {
      positive = stmt.positive;
      match = true;
      break;
    }
    else if (stmt.sub == "everybody") {
      positive = true;
      match = true;
      break;
    }
    else if (stmt.sub == "nobody") {
      positive = false;
      match = true;
      break;
    }
  }

  if (!match) {
    return "maybe.";
  }

  std::ostringstream oss;
  std::string sub = q.sub;
  if (sub == "I") {
    sub = "you";
  }
  else if (sub == "you") {
    sub = "I";
  }
  bool pl = plural(sub);
  if (positive) {
    oss << "yes, ";
    oss << sub <<" ";
    oss << q.pred;
    
    if (!pl) {
      oss << "s";
    }
    if (q.obj.size() > 0) {
      oss << " " << q.obj << ".";
    }
    else {
      oss << ".";
    }
  }
  else {
    oss << "no, ";
    oss << sub << " ";
    if (pl) {
      oss << "don't ";
    }
    else {
      oss << "doesn't ";
    }
    oss << q.pred;
    if (q.obj.size() > 0) {
      oss << " " << q.obj;
    }
    oss << ".";
  }

  return oss.str();
}

std::string AnswerWho(const Question& q, const Dialogue& d) 
{
  std::vector<std::string> subs;
  std::set<std::string> subjectSet;
  for (size_t i = 0; i < d.s.size(); i++) {
    const Statement& stmt = d.s[i];
    if (q.pred != stmt.pred || q.obj != stmt.obj) {
      continue;
    }
    if (stmt.sub == "nobody" || stmt.sub == "everybody") {
      subs.clear();
      subs.push_back(stmt.sub);
      break;
    }

    if (!stmt.positive) {
      continue;
    }
    std::string sub = stmt.sub;
    if (sub == "you") {
      sub = "I";
    }
    else if (sub == "I") {
      sub = "you";
    }

    if (subjectSet.find(sub) != subjectSet.end()) {
      continue;
    }
    else {
      subjectSet.insert(sub);
      subs.push_back(sub);
    }
  }

  if (subs.size() == 0) {
    return "I don't know.";
  }

  std::ostringstream oss;
  if (subs.size() == 1) {
    oss << subs[0] << " ";
  }
  else if (subs.size() == 2) {    
    oss << subs[0] << " and "<< subs[1]<<" ";
  }
  else {
    for (int i = 0; i<int(subs.size()) - 2; i++) {
      oss << subs[i] << ", ";
    }
    unsigned lastIdx = subs.size() - 1;
    oss << subs[lastIdx - 1] << " and " << subs[lastIdx] << " ";
  }
  oss << q.pred;
  if ( ! (plural(subs[0]) || subs.size() > 1) ) {
    oss << "s";
  }

  if (q.obj.size() > 0) {
    oss << " " << q.obj;
  }
  oss << ".";

  return oss.str();
}

std::string AnswerWhatDo(const Question& q, const Dialogue& d)
{
  std::string ans;

  std::vector<std::string> phraseVec;
  std::set<std::string> phraseSet;
  bool pl = plural(q.sub);
  for (size_t i = 0; i < d.s.size(); i++) {
    const Statement& stmt = d.s[i];
    bool positive = true;
    std::ostringstream phrasess;
    if (stmt.sub == "nobody") {
      positive = false;
    }
    else if (stmt.sub == "everybody") {
    
    }
    else if (stmt.sub == q.sub) {
      positive = stmt.positive;
    }
    else {
      continue;
    }
    if (!positive) {
      if (pl) {
        phrasess << "don't ";
      }
      else {
        phrasess << "doesn't ";
      }
    }

    phrasess << stmt.pred;
    if (positive && (!pl)) {
      phrasess << "s";
    }
    if (stmt.obj.size() > 0) {
      phrasess << " " << stmt.obj;
    }
    std::string phrase = phrasess.str();
    if (phraseSet.find(phrase) != phraseSet.end()) {
      continue;
    }
    else {
      phraseSet.insert(phrase);
      phraseVec.push_back(phrase);
    }
  }

  if (phraseVec.size() == 0) {
    return "I don't know.";
  }

  std::string sub = q.sub;
  if (sub == "I") {
    sub = "you";
  }
  else if (sub == "you") {
    sub = "I";
  }
  std::ostringstream oss;
  oss << sub <<" ";
  if (phraseVec.size() == 1) {
    oss << phraseVec[0] << ".";
  } else {
    for (unsigned i = 0; i < phraseVec.size() - 1; i++) {
      oss << phraseVec[i] << ", ";
    }
    unsigned lastIdx = phraseVec.size() - 1;
    oss << "and " << phraseVec[lastIdx] << ".";
  }

  return oss.str();
}

std::string Answer(const std::string& line, const Dialogue& d)
{
  if (d.contradiction) {
    return "I am abroad.";
  }
  std::string ans;

  Question q = ParseQuestion(line);

  if (q.marker == "do" || q.marker == "does") {
    ans = AnswerYesNo(q, d);
  }
  else if (q.marker == "who") {
    ans = AnswerWho(q, d);
  }
  else {
    ans = AnswerWhatDo(q, d);
  }
  return ans;
}

int sp13() {
//int main() {
  int T;
  std::cin >> T;
  for (int t = 0; t < T; t++) {
    std::string line;
    bool endOfDialogue = false;
    Dialogue d;
    std::cout << "Dialogue #" << int(t+1) << ":\n";
    std::string lastLine;
    while (!endOfDialogue) {
      std::getline(std::cin, line,'\n');
      if (line.size() < 2) {
        continue;
      }
      if (line.back() == '!') {
        lastLine = line;
        endOfDialogue = true;
        break;
      }

      char lastChar = line.back();
      if (lastChar == '.') {
        AddStmt(line, d);
      } else if (lastChar == '?') {
        std::cout << line <<"\n";
        std::string ans = Answer(line, d);
        std::cout << ans << "\n\n";
      }
    }
    std::cout << lastLine << "\n";
  }
  return 0;
}
