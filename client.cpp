#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <map>
#include <sstream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <fstream>
#include <ctime>
#include <boost/json.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <memory>
#include <chrono>
#include <Windows.h>
#include <commctrl.h>
#include "resource.h"
#include <comdef.h>
#include <boost/beast.hpp>
#include <locale>
#include <codecvt>
#include <vector>
#include "usage_headers/base64.h"
#include "usage_headers/md5.h"
#include "usage_headers/hmac.hpp"

struct DialogData {
	std::shared_ptr<boost::asio::ip::tcp::socket> socket;
	std::string dirs;
	std::string jwt;
};

void filename(std::string str, char* data) {

	str = str.substr(str.find_last_of('\\') + 1, str.length());

	strcpy(data, str.c_str());
}

void write_file(int size, char* data, char* filename_) {

	FILE* file;

	file = fopen(filename_, "wb");

	fwrite(data, 1, size, file);

	fclose(file);
}

std::string from_wchar_to_str(WCHAR* wstr, int wstrLen) {

	std::wstring wresult = wstr;

	std::string result(wresult.begin(), wresult.end());

	return result.substr(0, wstrLen);
}

std::string MD5Encode(std::string data) {

	MD5 md5;

	return md5(data);
}

std::string getFullPath(HWND hwndTV, HTREEITEM hItem) {

	std::string path;
	std::vector<char> buffer(MAX_PATH);
	TVITEM tvi;

	while (hItem != NULL) {

		tvi.mask = TVIF_TEXT;
		tvi.hItem = hItem;
		tvi.pszText = buffer.data();
		tvi.cchTextMax = buffer.size();

		if (TreeView_GetItem(hwndTV, &tvi)) {

			std::string strTvi = std::string(tvi.pszText);

			if (!path.empty()) {

				boost::format fmt("%1%\\");

				fmt% strTvi;

				strTvi = fmt.str();
			}

			boost::format fmt("%1%%2%");

			fmt% strTvi% path;

			path = fmt.str();
		}

		hItem = TreeView_GetParent(hwndTV, hItem);
	}

	return path;
}

std::vector<std::string> split(std::string str, char symbol) {

	std::vector<std::string> elems;
	std::istringstream iss(str);
	std::string item;

	while (std::getline(iss, item, symbol)) {

		elems.push_back(item);
	}

	return elems;
}

void addPathToTreeView(HWND hwndTV, std::string path, std::map<std::string, HTREEITEM>& treeItems) {

	HTREEITEM hPrev = TVI_ROOT;
	std::vector<std::string> folders = split(path, '\\');
	std::string cumulativePath;

	for (auto folder : folders) {

		boost::format fmt("%1%%2%");

		fmt% cumulativePath% folder;

		cumulativePath = fmt.str();

		if (treeItems.count(cumulativePath) == 0) {

			TVINSERTSTRUCT tvis;
			tvis.hParent = hPrev;
			tvis.hInsertAfter = TVI_LAST;
			tvis.item.mask = TVIF_TEXT;
			tvis.item.pszText = (LPSTR)folder.c_str();

			hPrev = (HTREEITEM)SendDlgItemMessage(hwndTV, IDC_TREE1, TVM_INSERTITEM, 0, (LPARAM)&tvis);
			treeItems[cumulativePath] = hPrev;
		}

		else {

			hPrev = treeItems[cumulativePath];
		}

		boost::format fmt_("%1%\\");

		fmt_% cumulativePath;

		cumulativePath = fmt_.str();
	}
}

void addPathsToTreeView(HWND hwndTV, std::string paths) {

	std::map<std::string, HTREEITEM> treeItems;
	std::vector<std::string> pathList = split(paths, '|');

	for (auto path : pathList) {
		addPathToTreeView(hwndTV, path, treeItems);
	}
}

std::string localPath() {
	WCHAR filename[MAX_PATH];
	GetModuleFileNameW(NULL, filename, MAX_PATH);
	std::wstring wpath = filename;
	std::string filepath(wpath.begin(), wpath.end());
	int endP = filepath.find_last_of('\\');
	filepath = filepath.substr(0, endP);
	return filepath;
}

BOOL CALLBACK DlgAuth(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK DlgMain(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{

		DialogData* data = reinterpret_cast<DialogData*>(lParam);

		std::string directoriesPath = data->dirs;

		addPathsToTreeView(hwnd, directoriesPath);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

	}
	return TRUE;

	case WM_CLOSE:
	{
		EndDialog(hwnd, 0);
	}
	return TRUE;

	case WM_COMMAND:
	{
	}
	return TRUE;

	case WM_NOTIFY:
	{

		LPNMHDR lpnmh = (LPNMHDR)lParam;
		if ((lpnmh->code == NM_DBLCLK) && (lpnmh->idFrom == IDC_TREE1))
		{
			boost::system::error_code ec;
			DialogData* data = reinterpret_cast<DialogData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

			try {

				HWND hTreeView = GetDlgItem(hwnd, 1001);

				HTREEITEM selectedItem = TreeView_GetSelection(hTreeView);

				std::string path = getFullPath(hTreeView, selectedItem);

				std::string str = data->jwt;

				std::istringstream iss(str);

				boost::property_tree::ptree pt;
				boost::property_tree::json_parser::read_json(iss, pt);

				pt.put("Path", path);

				std::ostringstream buf;
				boost::property_tree::write_json(buf, pt);

				std::string jsonStr = buf.str();

				boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb::post, "/Download", 11);
				request.set(boost::beast::http::field::content_type, "application/json");
				request.set(boost::beast::http::field::host, "PC");
				request.set(boost::beast::http::field::user_agent, "HTTP Client");
				request.body() = jsonStr;
				request.prepare_payload();
				boost::beast::http::write(*data->socket, request);

				TCHAR itemText[256];
				TVITEM item;
				item.hItem = selectedItem;
				item.mask = TVIF_TEXT;
				item.pszText = itemText;
				item.cchTextMax = 256;
				TreeView_GetItem(hTreeView, &item);

				boost::format fmt("%1%\\%2%");
				fmt% localPath() % itemText;

				boost::system::error_code ecRes;
				boost::beast::flat_buffer buffer;
				boost::beast::http::response<boost::beast::http::dynamic_body> response;
				boost::beast::http::read(*data->socket.get(), buffer, response);

				//auto cp = response[boost::beast::http::field::content_type];

				auto status = response.result_int();

				if (status == 200) {

					boost::property_tree::ptree pt_; // pt for getting "Size"
					std::istringstream iss_(boost::beast::buffers_to_string(response.body().data()));
					boost::property_tree::json_parser::read_json(iss_, pt_);

					std::size_t size = 0;
					size = pt_.get<std::size_t>("Size");

					std::ofstream file(itemText, std::ios::binary);
					std::size_t packet_size = 4096;
					std::vector<std::byte> package_buffer(packet_size);

					for (std::size_t offset = 0; offset < size; offset += packet_size)
					{
						std::size_t length = std::min(packet_size, size - offset);
						data->socket->receive(boost::asio::buffer(package_buffer, length));
						file.write(reinterpret_cast<const char*>(std::to_address(package_buffer.data())), length);
					}

					if (file.good()) {
						MessageBox(hwnd, fmt.str().c_str(), "Successfully download!", MB_OK);
					}
					else {
						MessageBox(hwnd, "Error writing to file", "Error!", MB_OK);
					}

					file.close();
				}

				else {
					MessageBox(hwnd, "Not file", "Attention", MB_OK);
				}

					/*boost::system::error_code ecRes;
					boost::beast::flat_buffer buffer;
					boost::beast::http::response<boost::beast::http::file_body> response;
					boost::beast::http::read(*data->socket.get(), buffer, response);

					response.body().open(fmt.str().c_str(), boost::beast::file_mode::write_new, ecRes);

					if (!ecRes) {
						MessageBox(hwnd, fmt.str().c_str(), "File writed!", MB_OK);
					}

					else {
						MessageBox(hwnd, ecRes.message().c_str(), "Error!", MB_OK);
					}*/

					//boost::property_tree::ptree pt_; // pt for getting "Size"
					//std::istringstream iss_(boost::beast::buffers_to_string(response.body().data()));
					//boost::property_tree::json_parser::read_json(iss_, pt_);
					//int size = 0;
					//size = pt_.get<int>("Size");
					//if (size != -1) {
					//	std::string str = pt_.get<std::string>("Data");
					//	//std::shared_ptr<char[]> fileData(new char[size]);
					//	//data->socket->receive(boost::asio::buffer(fileData.get(), size));
					//	TCHAR itemText[256];
					//	TVITEM item;
					//	item.hItem = selectedItem;
					//	item.mask = TVIF_TEXT;
					//	item.pszText = itemText;
					//	item.cchTextMax = 256;
					//	TreeView_GetItem(hTreeView, &item);
					//	//write_file(size, fileData.get(), itemText);
					//	write_file(size, str.data(), itemText);
					//	boost::format fmt("File %1% downloaded");
					//	fmt% itemText;
					//	MessageBox(hwnd, fmt.str().c_str(), "Download is complete", MB_OK);
					//}
					//else {
					//	boost::beast::flat_buffer buffer;
					//	boost::beast::http::response<boost::beast::http::dynamic_body> response;
					//	boost::beast::http::read(*data->socket, buffer, response);
					//	int status = response.result_int();
					//	if (status == 403) {
					//		MessageBox(hwnd, "File doesn`t exist or you are trying to download a folder", "Attention", MB_OK);
					//	}
					//}

			}
			catch (std::exception& e) {

				MessageBox(hwnd, e.what(), "Attention", MB_OK);
				data->socket.get()->close();
				EndDialog(hwnd, 0);
				DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgAuth);
			}

			catch (boost::system::system_error& e) {

				MessageBox(hwnd, e.what(), e.code().message().c_str(), MB_OK);
				data->socket.get()->close();
				EndDialog(hwnd, 0);
				DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgAuth);
			}

		}
	}
	return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK DlgAuth(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
	}
	return TRUE;

	case WM_CLOSE:
	{
		EndDialog(hwnd, 0);
	}
	return TRUE;

	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDOK) {

			boost::asio::io_context io_context;
			std::shared_ptr<boost::asio::ip::tcp::socket> socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
			boost::system::error_code ec;

			try {

				HWND IPField = GetDlgItem(hwnd, IDC_IPADDRESS1);
				HWND portField = GetDlgItem(hwnd, IDC_EDIT3);

				int IPFieldLen = GetWindowTextLength(IPField) + 1;
				int portFieldLen = GetWindowTextLength(portField);

				WCHAR wIP[256];
				WCHAR wport[256];

				GetWindowTextW(IPField, wIP, IPFieldLen);
				GetWindowTextW(portField, wport, portFieldLen);

				std::string IP = from_wchar_to_str(wIP, IPFieldLen);
				std::string port = from_wchar_to_str(wport, portFieldLen);

				int port_ = stoi(port);

				//boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(IP), port_);
				boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("192.168.100.14"), 8080);

				socket.get()->connect(ep, ec);

				if (!ec) {

					HWND loginField = GetDlgItem(hwnd, IDC_EDIT1);
					HWND passwordField = GetDlgItem(hwnd, IDC_EDIT2);

					int loginFieldLen = GetWindowTextLength(loginField);
					int passwordFieldLen = GetWindowTextLength(passwordField);

					WCHAR wlogin[256];
					WCHAR wpassword[256];

					GetWindowTextW(loginField, wlogin, loginFieldLen);
					GetWindowTextW(passwordField, wpassword, passwordFieldLen);

					/*std::string login = from_wchar_to_str(wlogin, loginFieldLen);
					std::string password = from_wchar_to_str(wpassword, passwordFieldLen);*/

					std::string login = "Admin";
					std::string password = "12345";

					boost::format fmtAuth("%1%|%2%");

					fmtAuth% login% password;

					std::string MD5_Content = MD5Encode(fmtAuth.str());

					boost::format fmtAuthJson("{\"Content-MD5\": \"%1%\"}");

					fmtAuthJson% MD5_Content;

					boost::beast::http::request<boost::beast::http::string_body> request(boost::beast::http::verb::post, "/Auth", 11);
					request.set(boost::beast::http::field::content_type, "application/json");
					request.set(boost::beast::http::field::host, "PC");
					request.set(boost::beast::http::field::user_agent, "HTTP Client");
					request.body() = fmtAuthJson.str();
					request.prepare_payload();
					boost::beast::http::write(*socket.get(), request);

					boost::beast::flat_buffer buffer;
					boost::beast::http::response<boost::beast::http::dynamic_body> response;
					boost::beast::http::read(*socket.get(), buffer, response);

					std::string newReqBody = boost::beast::buffers_to_string(response.body().data());

					std::istringstream iss(newReqBody);

					boost::property_tree::ptree pt;
					boost::property_tree::json_parser::read_json(iss, pt);

					boost::beast::http::request<boost::beast::http::string_body> request1(boost::beast::http::verb::post, "/Directories", 11);
					request1.set(boost::beast::http::field::content_type, "application/json");
					request1.set(boost::beast::http::field::host, "PC");
					request1.set(boost::beast::http::field::user_agent, "HTTP Client");
					request1.body() = newReqBody;
					request1.prepare_payload();
					boost::beast::http::write(*socket.get(), request1);

					boost::beast::flat_buffer buffer_;
					boost::beast::http::response<boost::beast::http::dynamic_body> response_;
					boost::beast::http::read(*socket.get(), buffer_, response_);

					std::string jsonDirs = boost::beast::buffers_to_string(response_.body().data());

					//boost::replace_all(jsonDirs, "\\", "\\\\");

					std::istringstream iss_(jsonDirs.data());

					boost::property_tree::ptree pt_;

					boost::property_tree::json_parser::read_json(iss_, pt_);

					std::string directories = pt_.get<std::string>("Directories");

					std::shared_ptr<DialogData> data(new DialogData);

					data.get()->socket = socket;
					data.get()->dirs = directories;
					data.get()->jwt = newReqBody;

					EndDialog(hwnd, 0);

					DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_MAIN), hwnd, DlgMain, (LPARAM)data.get());
				}

				else {

					MessageBox(hwnd, ec.message().c_str(), "Error", MB_OK);
				}
			}

			catch (const std::exception& e) {

				MessageBox(hwnd, e.what(), "Error", MB_OK);
			}
		}
	}
	return TRUE;
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgAuth);
}