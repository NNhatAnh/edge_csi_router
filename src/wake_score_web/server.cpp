#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <deque>
#include <regex>
#include <map>
#include <random>
#include "score_session.hpp"
using namespace std;
double nowms(){return chrono::duration<double,milli>(chrono::system_clock::now().time_since_epoch()).count();}
string quote(string s){string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\')o+='\\',o+=c;else if(c>=32)o+=c;}return o+'"';}
string field(const string&s,const string&k){smatch m;if(regex_search(s,m,regex("\""+k+"\"\\s*:\\s*\"([^\"]*)\"")))return m[1];return "";}
string numeric(const string&s,const string&k){smatch m;if(regex_search(s,m,regex("\""+k+"\"\\s*:\\s*([-+]?[0-9]+(?:\\.[0-9]*)?(?:[eE][-+]?[0-9]+)?)")))return m[1];return "";}
string join(const deque<string>&q){string s="[";for(auto&v:q){if(s.size()>1)s+=',';s+=v;}return s+"]";}
volatile sig_atomic_t alive=1;void halt(int){alive=0;}
int main(int argc,char**argv){
 int port=argc>2?stoi(argv[2]):8081;
 const bool scoreMode = port == 8082;
 string base=argc>1?argv[1]:"/tmp/wake_m1",html;
 {ifstream f(scoreMode && argc>3 ? argv[3] : base+(scoreMode ? "/web/score.html" : "/web/air.html"));
  html=string(istreambuf_iterator<char>(f),{});}
 if(html.empty())return 1;
 string diagnostic;{ifstream f(base+"/web/diagnostics.html");diagnostic=string(istreambuf_iterator<char>(f),{});}
 string gioithieu;{ifstream f(base+"/web/gioithieu.html");gioithieu=string(istreambuf_iterator<char>(f),{});if(gioithieu.empty()){ifstream g(base+"/gioithieu.html");gioithieu=string(istreambuf_iterator<char>(g),{});}}
 string qrJs;{ifstream f(base+"/web/qrcode.min.js");qrJs=string(istreambuf_iterator<char>(f),{});if(qrJs.empty()){ifstream g(base+"/qrcode.min.js");qrJs=string(istreambuf_iterator<char>(g),{});}}
 const map<string,string> modelFiles={{"M1","M1_logistic.txt"},{"M2","M2_rf.txt"},{"M3","M3_boost.txt"},{"M4","M4_rf_all.txt"},{"M5","M5_logistic.txt"},{"M5R","M5R_logistic.txt"}};
 int sock=socket(AF_INET,SOCK_STREAM,0),one=1;setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);addr.sin_addr.s_addr=htonl(INADDR_ANY);if(bind(sock,(sockaddr*)&addr,sizeof(addr))||listen(sock,8)){perror("web listen");return 1;}
 signal(SIGTERM,halt);signal(SIGINT,halt);signal(SIGPIPE,SIG_IGN);
 random_device rd;string token=to_string(rd())+to_string(rd());pid_t child=0;int input=-1;string buffer,state="STOPPED",reason="",error="",lastEvent="",currentModel="M5R",lastScore="",motionScore="",motionHigh="",windowSpan="",maxGap="",stillElapsed="0",phaseElapsed="0",currentMac="",currentIp="";double last=0;int label=-1,yes=0,no=0,seq=0;bool unavailable=true;deque<string> points,history;
 ScoreSession scores;
 constexpr int scoreStep = 10;
 auto stop=[&](){if(child){kill(-child,SIGTERM);for(int i=0;i<30;i++){if(waitpid(child,nullptr,WNOHANG)==child){child=0;break;}usleep(100000);}if(child){kill(-child,SIGKILL);waitpid(child,nullptr,0);child=0;}}if(input>=0)close(input);input=-1;state="STOPPED";unavailable=true;};
 auto scoreEvent = [&](const string& line) {
    const double received = nowms();
    if (field(line, "status") == "unavailable") {
        scores.gap();
        reason = field(line, "reason");
        unavailable = true;
        return;
    }
    string value = numeric(line, "score");
    string binary = numeric(line, "label");
    if (value.empty() || (binary != "0" && binary != "1")) return;
    const double score = strtod(value.c_str(), nullptr);
    if (!isfinite(score) || score < 0 || score > 1) return;
    string timestamp = numeric(line, "timestamp_s");
    string gap = numeric(line, "max_gap_s");
    string span = numeric(line, "window_span_s");
    static const regex poorQuality(R"("quality_ok"\s*:\s*false)");
    static const regex retainedGap(R"("retained_gap_ok"\s*:\s*true)");
    const bool continuous = !regex_search(line, poorQuality) &&
                            (gap.empty() || strtod(gap.c_str(), nullptr) <= ScoreWake::gapLimit);
    const bool valid = continuous || regex_search(line, retainedGap);
    const double source = timestamp.empty() ? received / 1000 : strtod(timestamp.c_str(), nullptr);
    if (!isfinite(source) || source < 0) return;
    if (continuous && !span.empty()) {
        const double seconds = strtod(span.c_str(), nullptr);
        scores.gapTimeout = clamp(1.5 * seconds * scoreStep / 39, ScoreWake::gapLimit, 2.4);
    }
    scores.push(source, received, binary[0] - '0', valid, score);
    string pt = "[" + to_string(scores.sequence) + "," + to_string(received) + "," + binary + "," + value + ",0]";
    points.push_back(pt);
    if (points.size() > 600) points.pop_front();
    last = received;
    lastScore = value;
    label = stoi(binary);
    maxGap = gap;
    windowSpan = span;
    unavailable = !valid;
    reason = valid ? "" : "csi_gap";
 };
 auto event=[&](string line){if(line.empty())return;if(line[0]!='{'){if(line.rfind("decoded=",0)!=0)error=line.substr(0,300);return;}if(scoreMode){scoreEvent(line);return;}string ev=field(line,"event");string m=field(line,"model");if(m=="M1"||m=="M2"||m=="M3"||m=="M4"||m=="M5"||m=="M5R")currentModel=m;if(field(line,"status")=="unavailable"){unavailable=true;reason=field(line,"reason");return;}string n=numeric(line,"label"),score=numeric(line,"score");if(!score.empty())lastScore=score;motionScore=numeric(line,"motion_score");motionHigh=numeric(line,"motion_high");string span=numeric(line,"window_span_s"),gap=numeric(line,"max_gap_s");if(!span.empty())windowSpan=span;if(!gap.empty())maxGap=gap;stillElapsed=numeric(line,"still_elapsed_s");phaseElapsed=numeric(line,"phase_elapsed_s");if(stillElapsed.empty())stillElapsed="0";if(phaseElapsed.empty())phaseElapsed="0";if(n=="0"||n=="1"){label=stoi(n);last=nowms();unavailable=false;points.push_back("["+to_string(last)+","+n+"]");if(points.size()>1200)points.pop_front();}string st=field(line,"state");if(!st.empty())state=st;reason=field(line,"reason");
    if (ev == "WAKE" || ev == "FAIL") {
      (ev == "WAKE") ? yes++ : no++;
      string r;
      if (ev == "WAKE") r = "Đủ hai nét hợp lệ và đã giữ yên cuối";
      else if (reason == "draw_timeout") r = "Quá 3 giây tính từ lúc bắt đầu vẽ";
      else if (reason == "pause_timeout") r = "Dừng giữa hai nét quá lâu";
      else if (reason == "extra_motion") r = "Có thêm chuyển động khi đang xác nhận kết thúc";
      else if (reason == "weak_stroke") r = "Nét quá ngắn hoặc chuyển động quá nhẹ";
      else if (reason == "unobserved_interval") r = "CSI gián đoạn: có khoảng trống >0,5 giây, cửa sổ 40 gói >2,4 giây hoặc ngừng CSI >2 giây";
      else if (reason == "invalid_time") r = "Timestamp CSI không hợp lệ";
      else r = "Không nhận diện (" + reason + ")";

      lastEvent = "{\"id\":" + to_string(++seq) + ",\"time\":" + to_string(nowms()) + ",\"ok\":" + (ev == "WAKE" ? "true" : "false") + ",\"reason\":" + quote(r) + ",\"model\":" + quote(currentModel) + ",\"mac\":" + quote(currentMac) + ",\"ip\":" + quote(currentIp) + "}";
      history.push_back(lastEvent);
      if (history.size() > 200) history.pop_front();
    }
  };
 while(alive){pollfd fds[2]={{sock,POLLIN,0},{input,POLLIN,0}};poll(fds,input>=0?2:1,100);
  if(input>=0&&(fds[1].revents&(POLLIN|POLLHUP))){char b[8192];ssize_t n;while((n=read(input,b,sizeof(b)))>0){buffer.append(b,n);size_t pos;while((pos=buffer.find('\n'))!=string::npos){event(buffer.substr(0,pos));buffer.erase(0,pos+1);}if(buffer.size()>65536)buffer.clear();}}
  if(child&&waitpid(child,nullptr,WNOHANG)==child){child=0;if(input>=0)close(input);input=-1;state="STOPPED";unavailable=true;if(error.empty())error="Bộ thu đã dừng. Kiểm tra kết nối và chương trình thu khác.";}
  if (scoreMode && child && scores.lastReceived >= 0 &&
      nowms() - scores.lastReceived > scores.gapTimeout * 1000) {
      scores.gap();
      unavailable = true;
      if (reason.empty()) reason = "csi_gap";
  }
  if (!(fds[0].revents & POLLIN)) continue;
  int c = accept(sock, nullptr, nullptr);
  if (c < 0) continue;
  timeval timeout{2, 0};
  setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  string req;
  char b[4096];
  size_t boundary = string::npos;
  int length = 0;
  while (req.size() < 8192) {
    ssize_t n = recv(c, b, sizeof(b), 0);
    if (n <= 0) break;
    req.append(b, n);
    boundary = req.find("\r\n\r\n");
    if (boundary != string::npos) {
      smatch m;
      if (regex_search(req, m, regex("Content-Length: ([0-9]+)", regex::icase))) length = stoi(m[1]);
      if (length > 4096) break;
      if (req.size() >= boundary + 4 + length) break;
    }
  }
  istringstream first(req);string method,path;first>>method>>path;string body=boundary==string::npos?"":req.substr(boundary+4);string result="{}",type="application/json; charset=utf-8";int status=200;
  if(method=="OPTIONS"){status=204;result="";type="text/plain";}
  string cleanPath = path;
  size_t qpos = cleanPath.find('?');
  if (qpos != string::npos) cleanPath = cleanPath.substr(0, qpos);
  const string scoreCursor = "/api/score?after=";
  if (scoreMode && method == "GET" && (cleanPath == "/api/score" || path.rfind(scoreCursor, 0) == 0)) {
    result = "{\"version\":\"8082_v6\",\"token\":" + quote(token) + ",\"session\":" + to_string(scores.session) +
        ",\"running\":" + (child ? "true" : "false") +
        ",\"stale\":" + (unavailable ? "true" : "false") +
        ",\"model\":" + quote(currentModel) + ",\"ip\":" + quote(currentIp) +
        ",\"state\":" + quote(child ? scores.wake.state() : "STOPPED") +
        ",\"stroke_progress\":" + to_string(child ? scores.wake.progress : 0) +
        ",\"stroke_fraction\":" + to_string(child ? scores.wake.strokeFraction(scores.observedTime) : 0) +
        ",\"score\":" + (lastScore.empty() ? "null" : lastScore) +
        ",\"label\":" + to_string(label) +
        ",\"window_span_s\":" + (windowSpan.empty() ? "null" : windowSpan) +
        ",\"max_gap_s\":" + (maxGap.empty() ? "null" : maxGap) +
        ",\"reason\":" + quote(reason) + ",\"error\":" + quote(error) +
        ",\"accepted\":" + to_string(scores.accepted) +
        ",\"rejected\":" + to_string(scores.rejected) +
        ",\"first_id\":" + to_string(scores.sequence + 1) +
        ",\"last_id\":" + to_string(scores.sequence) +
        ",\"result_reason\":" + quote(scores.resultReason) + ",\"points\":" + join(points) + ",\"events\":[]}";
  }
  else if((cleanPath=="/"||cleanPath=="/air"||(scoreMode&&cleanPath=="/diagnostics"))&&method=="GET"){result=html;type="text/html; charset=utf-8";}
  else if((cleanPath=="/gioithieu"||cleanPath=="/gioithieu.html")&&method=="GET"){result=gioithieu.empty()?html:gioithieu;type="text/html; charset=utf-8";}
  else if((cleanPath=="/qrcode.min.js"||cleanPath=="/web/qrcode.min.js")&&method=="GET"){result=qrJs;type="application/javascript; charset=utf-8";}
  else if(cleanPath=="/diagnostics"&&method=="GET"){result=diagnostic;type="text/html; charset=utf-8";}
  else if(cleanPath=="/api/models"&&method=="GET"){result="[";for(auto& item:modelFiles){if(scoreMode&&item.first!="M5R")continue;if(access((base+"/models/"+item.second).c_str(),R_OK))continue;if(result.size()>1)result+=',';result+=quote(item.first);}result+=']';}
  else if(cleanPath=="/api/status"&&method=="GET"){result="{\"token\":"+quote(token)+",\"mac\":"+quote(currentMac)+",\"ip\":"+quote(currentIp)+",\"still_elapsed_s\":"+stillElapsed+",\"phase_elapsed_s\":"+phaseElapsed+",\"running\":"+(child?"true":"false")+",\"model\":"+quote(currentModel)+",\"score\":"+(lastScore.empty()?"null":lastScore)+",\"motion_score\":"+(motionScore.empty()?"null":motionScore)+",\"motion_high\":"+(motionHigh.empty()?"null":motionHigh)+",\"window_span_s\":"+(windowSpan.empty()?"null":windowSpan)+",\"max_gap_s\":"+(maxGap.empty()?"null":maxGap)+",\"recognition_mode\":\"rhythm\""+",\"reason\":"+quote(reason)+",\"state\":"+quote(state)+",\"label\":"+to_string(label)+",\"stale\":"+((unavailable||nowms()-last>2000)?"true":"false")+",\"error\":"+quote(error)+",\"yes\":"+to_string(yes)+",\"no\":"+to_string(no)+",\"points\":"+join(points)+",\"history\":"+join(history)+"}";}
  else if(cleanPath=="/api/devices"&&method=="GET"){result="[";FILE*f=popen("iw dev phy00.0-0 station dump | awk '/^Station /{print $2}'","r");if(f){char mac[80];while(fgets(mac,sizeof(mac),f)){string m=mac;m.erase(m.find_last_not_of("\r\n")+1);if(!regex_match(m,regex("[0-9a-fA-F:]{17}")))continue;string cmd="ip -4 neigh show | awk -v m='"+m+"' 'tolower($0) ~ m {print $1;exit}'";FILE*g=popen(cmd.c_str(),"r");char ip[80]{};if(g){if(fgets(ip,sizeof(ip),g)!=nullptr){}pclose(g);}string a=ip;if(!a.empty())a.erase(a.find_last_not_of("\r\n")+1);if(result.size()>1)result+=',';result+="{\"mac\":"+quote(m)+",\"ip\":"+quote(a)+"}";}pclose(f);}result+=']';}
  else if(method=="POST"&&(cleanPath=="/api/start"||cleanPath=="/api/stop"||cleanPath=="/api/clear")){
   string headers=req.substr(0,boundary);for(char&v:headers)v=tolower((unsigned char)v);if(headers.find("x-wake-token: "+token)==string::npos){status=403;result="{\"error\":\"Yêu cầu không hợp lệ\"}";}
   else if(cleanPath=="/api/stop"){stop();}
   else if(cleanPath=="/api/clear"){
       if (scoreMode && child) {status=409;result="{\"error\":\"Dừng phiên thu trước khi xóa lịch sử\"}";}
       else if (scoreMode) {scores.clear();lastScore.clear();label=-1;}
       else {history.clear();yes=no=0;lastEvent.clear();}
   }
   else if(child){status=409;result="{\"error\":\"Đang thu, hãy dừng trước\"}";}
  else {string mac=field(body,"mac"),ip=field(body,"ip"),ping=scoreMode ? "10" : numeric(body,"ping"),model=scoreMode ? "M5R" : field(body,"model");in_addr parsed{};
    string step=scoreMode ? to_string(scoreStep) : "5";
    ScoreConfig config;
    if(model!="M1"&&model!="M2"&&model!="M3"&&model!="M4"&&model!="M5"&&model!="M5R"){status=400;result="{\"error\":\"Chọn model M1, M2, M3, M4, M5 hoặc M5R\"}";}
    else if(access((base+"/models/"+modelFiles.at(model)).c_str(),R_OK)){status=400;result="{\"error\":\"Model không có trên router\"}";}
    else if(!regex_match(mac,regex("([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}"))||inet_pton(AF_INET,ip.c_str(),&parsed)!=1||ping.empty()||ping.size()>2||stoi(ping)<1||stoi(ping)>20){status=400;result="{\"error\":\"Kiểm tra thiết bị, IP và ping 1–20 ms\"}";}
    else if(access("/tmp/cfr_spool.lock",F_OK)==0){status=409;result="{\"error\":\"Nguồn CSI đang được dùng. Dừng script terminal hoặc giao diện thu khác trước.\"}";}
    else{int pipes[2];if(pipe(pipes)){status=500;}else{error.clear();buffer.clear();points.clear();label=-1;lastScore="";motionScore.clear();motionHigh.clear();windowSpan="";maxGap="",stillElapsed="0",phaseElapsed="0",currentMac="",currentIp="";reason="";last=0;unavailable=true;state="WAIT_STILL";currentModel=model;currentMac=mac;currentIp=ip;stillElapsed=phaseElapsed="0";child=fork();if(child==0){setpgid(0,0);dup2(pipes[1],1);dup2(pipes[1],2);close(pipes[0]);close(pipes[1]);close(sock);close(c);setenv("WAKE_OUTPUT_FORMAT","json",1);setenv("WAKE_PAUSE_MAX","3",1);string script=base+"/capture_wake.sh";execl("/bin/sh","sh",script.c_str(),"--model",model.c_str(),"--mac",mac.c_str(),"--ip",ip.c_str(),"--ping-ms",ping.c_str(),"--bw","80","--step-packets",step.c_str(),(char*)nullptr);_exit(127);}close(pipes[1]);if(child<0){close(pipes[0]);child=0;status=500;}else{if(scoreMode)scores.start(config);setpgid(child,child);input=pipes[0];fcntl(input,F_SETFL,O_NONBLOCK);}}}
   }
  }else{status=404;result="{}";}
  string response="HTTP/1.1 "+to_string(status)+" Response\r\nContent-Type: "+type+"\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, X-Wake-Token\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "+to_string(result.size())+"\r\n\r\n"+result;size_t sent=0;while(sent<response.size()){ssize_t n=send(c,response.data()+sent,response.size()-sent,MSG_NOSIGNAL);if(n<=0)break;sent+=n;}close(c);
 }
 stop();close(sock);
}
