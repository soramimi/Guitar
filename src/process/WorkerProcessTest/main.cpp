#include "ProcessWinConPtyWithWorker.h"
#include "ConPtyWorkerProtocol.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
static void check(bool ok, char const *what) { if (!ok) throw std::runtime_error(what); }
static std::string drain(ProcessWinConPtyWithWorker &p) {
	std::string s; char b[4096]; int n; while ((n = p.read_output(b, sizeof(b))) > 0) s.append(b, n); return s;
}
int main(int argc, char **argv) {
	if (argc > 1 && std::string(argv[1]) == "--conpty-worker-v1") {
		if (GetEnvironmentVariableA("WORKER_TEST_STALL",nullptr,0)) Sleep(60000);
		return 99;
	}
	if (argc > 1 && std::string(argv[1]) == "--abandon-session") {
		char path[MAX_PATH]; GetModuleFileNameA(nullptr,path,sizeof(path));
		ProcessWinConPtyWithWorker p; p.start(std::string("\"")+path+"\" --child pid-sleep", "", true);
		if (!p.wait_for_output("PID:")) return 8;
		Sleep(50); auto out=drain(p); fwrite(out.data(),1,out.size(),stdout); fflush(stdout);
		ExitProcess(0); // Deliberately bypass destructors: job handle closure must do the cleanup.
	}
	if (argc > 1 && std::string(argv[1]) == "--child") {
		SetConsoleOutputCP(CP_UTF8); std::string mode = argc > 2 ? argv[2] : "";
		if (mode == "descendant") {
			char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, sizeof(exe));
			std::string cmd = std::string("\"") + exe + "\" --child sleep";
			STARTUPINFOA si {}; si.cb = sizeof(si); PROCESS_INFORMATION pi {};
			if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return 9;
			printf("DESCENDANT:%lu\n", pi.dwProcessId); fflush(stdout); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 0;
		}
		if (mode == "pid-sleep") { printf("PID:%lu\n", GetCurrentProcessId()); fflush(stdout); Sleep(60000); return 0; }
		if (mode == "sleep") { Sleep(60000); return 0; }
		if (mode == "input") { char b[80]; fgets(b, sizeof(b), stdin); printf("INPUT:%s", b); return 0; }
		if (mode == "size") {
			for (int i = 0; i < 2; ++i) {
				CONSOLE_SCREEN_BUFFER_INFO info {}; GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
				printf("SIZE:%dx%d\n", info.dwSize.X, info.dwSize.Y); fflush(stdout); Sleep(400);
			} return 0;
		}
		if (mode == "environment") { char b[200]; GetEnvironmentVariableA("WORKER_TEST", b, sizeof(b)); printf("ENV:%s\n", b); GetCurrentDirectoryA(sizeof(b), b); printf("DIR:%s\n", b); return 0; }
		if (mode == "large") { for (int i=0; i<15000; ++i) printf("line-%05d\n", i); printf("FINAL-MARKER\n"); return 0; }
		DWORD flags = 0; HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE); GetConsoleMode(out, &flags); SetConsoleMode(out, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		printf("\x1b[31mCOLOR\x1b[0m UTF8:\xe6\x97\xa5\xe6\x9c\xac\nFINAL-MARKER\n"); fflush(stdout); return 37;
	}
	try {
		char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, sizeof(path));
		std::string command = std::string("\"") + path + "\" --child ";
		ProcessWinConPtyWithWorker p; std::atomic<int> completed {0};
		p.set_completion_callback([&](bool, std::shared_ptr<void>) { ++completed; }, {});
		p.start(command + "output", "", true); auto r = p.wait(10000); check(!r.running() && r.exit_code() == 37, "exit code");
		auto output = drain(p); check(output.find("FINAL-MARKER") != std::string::npos, "final drain");
		check(output.find("\x1b[31m") != std::string::npos, "ANSI retention"); check(output.find("\xe6\x97\xa5\xe6\x9c\xac") != std::string::npos, "UTF8"); p.stop(); check(completed == 1, "completion count"); puts("PASS output/exit/ANSI/UTF8/completion");
		p.start(command + "large", "", true); r = p.wait(15000); check(!r.running(), "large timeout"); output = drain(p); check(output.find("FINAL-MARKER") != std::string::npos, "large final drain"); puts("PASS large final output");
		p.resize_terminal(91, 33); p.start(command + "size", "", true);
		check(p.wait_for_output("SIZE:91x33"), "initial size"); p.resize_terminal(103, 41); r=p.wait(10000); output=drain(p); check(output.find("SIZE:103x41") != std::string::npos,"resize"); puts("PASS initial size/resize");
		wchar_t directory[MAX_PATH]; GetTempPathW(MAX_PATH, directory); p.set_change_dir(directory);
		p.start(command + "environment", "WORKER_TEST=propagated", true); r=p.wait(10000); check(!r.running(),"env timeout"); output=drain(p); check(output.find("ENV:propagated") != std::string::npos,"environment"); char expected[MAX_PATH]; WideCharToMultiByte(CP_UTF8, 0, directory, -1, expected, MAX_PATH, nullptr, nullptr);
		std::string prefix = std::string("DIR:") + expected; while (prefix.back() == '\\') prefix.pop_back();
		check(output.find(prefix) != std::string::npos, "working directory"); p.set_change_dir(L""); puts("PASS environment/working directory");
		p.start(command + "input", "", true); p.write_input("hello\r",6); r=p.wait(10000); check(!r.running(),"input timeout"); check(drain(p).find("INPUT:hello") != std::string::npos,"input transfer"); puts("PASS input");
		p.start(command + "sleep", "", true); check(p.wait(10).running(), "timeout semantics"); p.stop(); check(!p.is_running(),"stop"); puts("PASS timeout/stop");
		p.set_capture_output_vector(false); p.set_max_output_queue_size(64); p.start(command+"large", "",true); r=p.wait(15000); check(!r.running(), "queue timeout"); check(p.stdout_bytes().empty(),"capture disabled"); check(drain(p).size()<=64,"queue bound"); puts("PASS bounded queue/capture disabled");
		p.start("Z:\\does-not-exist.exe", "", true); r=p.wait(10000); check(!r.running() && !r.error_message().empty(),"child failure"); p.stop(); puts("PASS child launch failure");
		p.set_capture_output_vector(true); p.set_max_output_queue_size(0);
		p.start(command+"descendant", "", true); r=p.wait(10000); check(!r.running(),"descendant timeout"); output=drain(p);
		auto at=output.find("DESCENDANT:"); check(at!=std::string::npos,"descendant pid");
		DWORD pid=static_cast<DWORD>(std::stoul(output.substr(at+11))); HANDLE child=OpenProcess(SYNCHRONIZE,FALSE,pid);
		if(child) { check(WaitForSingleObject(child,2000)==WAIT_OBJECT_0,"orphan descendant"); CloseHandle(child); }
		puts("PASS descendant cleanup");
		{
			SECURITY_ATTRIBUTES sa {sizeof(sa),nullptr,TRUE}; HANDLE read=nullptr,write=nullptr;
			check(CreatePipe(&read,&write,&sa,0)!=0,"host pipe"); SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
			STARTUPINFOA si {}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=write; si.hStdError=write;
			PROCESS_INFORMATION pi {}; std::string cmd=std::string("\"")+path+"\" --abandon-session";
			check(CreateProcessA(nullptr,cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0,"host spawn");
			CloseHandle(write); CloseHandle(pi.hThread); check(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0,"host exit"); CloseHandle(pi.hProcess);
			std::string bytes; char buf[4096]; DWORD n; while(ReadFile(read,buf,sizeof(buf),&n,nullptr) && n) bytes.append(buf,n); CloseHandle(read);
			auto pos=bytes.find("PID:"); check(pos!=std::string::npos,"host child pid"); DWORD id=static_cast<DWORD>(std::stoul(bytes.substr(pos+4)));
			HANDLE orphan=OpenProcess(SYNCHRONIZE,FALSE,id); if(orphan) { check(WaitForSingleObject(orphan,2000)==WAIT_OBJECT_0,"host death cleanup"); CloseHandle(orphan); }
			puts("PASS host death cleanup");
		}
		for(int i=0;i<5;i++) { p.start(command+"sleep", "",true); p.stop(); }
		puts("PASS immediate repeated stop/restart");
		std::wstring own_path(path,path+strlen(path)); p.set_worker_path(own_path);
		p.start(command+"output", "",true); r=p.wait(10000); check(!r.running() && !r.error_message().empty(),"worker crash"); p.stop(); puts("PASS worker abnormal exit");
		SetEnvironmentVariableA("WORKER_TEST_STALL","1");
		p.start(command+"output", "",true); auto before=std::chrono::steady_clock::now(); p.stop();
		SetEnvironmentVariableA("WORKER_TEST_STALL",nullptr);
		check(std::chrono::steady_clock::now()-before<std::chrono::seconds(6),"forced stop deadline"); check(!p.is_running(),"forced stop"); puts("PASS forced stop deadline");
		int count=completed; p.set_worker_path(L"Z:\\missing-worker.exe"); p.start(command+"output", "", true); r=p.wait(100); check(!r.running() && !r.started() && !r.error_message().empty(),"worker failure"); check(completed==count+1,"failure completion"); puts("PASS worker launch failure");
		namespace wire=process::worker_protocol;
		std::string packet=wire::frame(wire::Input,"abc"), partial=packet.substr(0,15), body; wire::Type type;
		check(!wire::take(partial,type,body),"partial header"); partial+=packet.substr(15); check(wire::take(partial,type,body) && body=="abc","fragmented frame");
		packet[0]=0; bool rejected=false; try { wire::take(packet,type,body); } catch(std::exception const &) { rejected=true; } check(rejected,"bad magic");
		packet=wire::frame(wire::Input); packet[12]=char(0xff); packet[13]=char(0xff); packet[14]=char(0xff); packet[15]=char(0x7f);
		rejected=false; try { wire::take(packet,type,body); } catch(std::exception const &) { rejected=true; } check(rejected,"oversized frame");
		puts("PASS protocol fragmentation/validation");
		return 0;
	} catch (std::exception const &e) { fprintf(stderr,"FAIL %s\n",e.what()); return 1; }
}

