// Antilua
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "session.h"
#include "filesys.h"
#include "log.h"
#include "porting.h"

#include <json/json.h>

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cerrno>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace session {

std::string getSessionDir()
{
#ifdef _WIN32
	return fs::TempPath() + "\\antilua";
#else
	// Try XDG_RUNTIME_DIR first
	const char *runtime = std::getenv("XDG_RUNTIME_DIR");
	if (runtime && runtime[0])
		return std::string(runtime) + "/antilua";

	return std::string("/tmp/antilua-") + std::to_string(geteuid());
#endif
}

static std::string sessionFilePath()
{
	return getSessionDir() + "/session";
}

#ifndef _WIN32
static bool validateSessionDir(const std::string &dir)
{
	struct stat st;
	return lstat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
		st.st_uid == geteuid() && (st.st_mode & 0077) == 0;
}

static bool ensureSessionDir(const std::string &dir)
{
	if (mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST)
		return false;
	struct stat st;
	if (lstat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) ||
			st.st_uid != geteuid() || chmod(dir.c_str(), 0700) != 0)
		return false;
	return validateSessionDir(dir);
}
#endif

void write(pid_type pid, const std::string &pipe_lua_path)
{
	std::string dir = getSessionDir();
#ifdef _WIN32
	if (!fs::CreateAllDirs(dir)) {
#else
	if (!ensureSessionDir(dir)) {
#endif
		warningstream << "session: cannot create directory "
			<< dir << std::endl;
		return;
	}

	Json::Value root;
	root["pid"] = (Json::Value::Int64)pid;
	root["pipe_lua_path"] = pipe_lua_path;
	root["timestamp"] = (Json::Value::UInt64)std::time(nullptr);

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	std::string json = Json::writeString(builder, root) + "\n";

	const std::string path = sessionFilePath();
#ifdef _WIN32
	std::ofstream ofs(path);
	if (ofs)
		ofs << json;
	else
		warningstream << "session: cannot write " << path << std::endl;
#else
	int flags = O_WRONLY | O_CREAT;
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	int fd = open(path.c_str(), flags, 0600);
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
			st.st_uid != geteuid() || fchmod(fd, 0600) != 0 || ftruncate(fd, 0) != 0) {
		if (fd >= 0)
			close(fd);
		warningstream << "session: refusing unsafe file " << path << std::endl;
		return;
	}
	size_t offset = 0;
	while (offset < json.size()) {
		ssize_t written = ::write(fd, json.data() + offset, json.size() - offset);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0) {
			warningstream << "session: incomplete write to " << path << std::endl;
			break;
		}
		offset += written;
	}
	close(fd);
#endif
}

Info read()
{
	Info info;

#ifndef _WIN32
	if (!validateSessionDir(getSessionDir()))
		return info;
#endif

	std::ifstream ifs(sessionFilePath());
	if (!ifs)
		return info;

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
		warningstream << "session: parse error: " << errs << std::endl;
		return info;
	}

	info.pid = root.get("pid", 0).asInt();
	info.pipe_lua_path = root.get("pipe_lua_path", "").asString();
	info.timestamp = root.get("timestamp", 0).asUInt64();
	return info;
}

bool isLive()
{
	Info info = read();
	if (info.pid <= 0)
		return false;
	return porting::pid_alive(info.pid);
}

void remove()
{
#ifndef _WIN32
	if (!validateSessionDir(getSessionDir()))
		return;
#endif
	std::string path = sessionFilePath();
	// Don't warn on ENOENT
	// Only warn on non-ENOENT errors
	if (fs::PathExists(path) && !fs::DeleteSingleFileOrEmptyDirectory(path, false))
		warningstream << "session: cannot remove " << path << std::endl;
}

} // namespace session
