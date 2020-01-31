#include "stdafx.h"

#include <iostream>
#include <sstream>
#include <array>
#include <string>
#include <vector>
#include <random>

struct options_t {
    // list of window titles or pids as strings
    std::vector<std::string> window_data_to_search {};

    // all of the windows located
    // pair<class name, title> for FindWindowA
    std::vector<std::pair<std::string, std::string >> windows {};

    // string to replace the original window title with
    std::string spoof_string {};

    // -v switch
    bool only_check_visible_windows = false;

    // internal bools
    bool using_titles = false;
    bool using_pids = false;
};

enum class status_type_t : int {
    SUCCESS,
    FAILURE_WITH_ERROR_CODE,
    FAILURE,
};

std::string gle_error_str() {
    LPSTR msg_buffer = nullptr;
    size_t size = FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&msg_buffer, 0, NULL );
    std::string message( msg_buffer, size );
    LocalFree( msg_buffer );

    return message;
}

void print_status_msg( status_type_t status, std::stringstream& stream ) {

    switch( status ) {
        case status_type_t::SUCCESS:
        {
            std::cout << "[+] ";
            break;
        }
        case status_type_t::FAILURE_WITH_ERROR_CODE:
        case status_type_t::FAILURE:
        {
            std::cout << "[-] ";
            break;
        }
    }

    std::cout << stream.str().c_str();

    // clear the stream's data and flags
    stream.str( std::string() );
    stream.clear();

    if( status == status_type_t::SUCCESS || status == status_type_t::FAILURE ) {
        std::cout << std::endl;
        return;
    }

    std::cout << std::hex << " | Error Code - " << GetLastError() << ": " << gle_error_str().data() << std::endl;
}

std::vector<std::string> parse_semicolon_seperated_string( std::string str ) {
    std::vector<std::string> ret {};

    size_t pos = 0;
    std::string part {};
    while( ( pos = str.find( ';' ) ) != std::string::npos ) {
        part = str.substr( 0, pos );
        ret.push_back( part );
        str.erase( 0, pos + 1 );
    }

    ret.push_back( str );
    return ret;
}

std::string get_random_string() {
    const std::string charset { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" };

    // can't go wrong with mersenne twister
    std::random_device rd {};
    std::mt19937 mt( rd() );

    // - 1 to ignore the null char at the end
    std::uniform_int_distribution<int> char_grab( 0, charset.size() - 1 );

    // set minimum length = 10, max length = 32
    std::uniform_int_distribution<int> choose_len( 10, 32 );

    const int str_length = choose_len( mt );

    std::string str( str_length, 0 );
    std::generate_n( str.begin(), str_length, [ charset, &char_grab, &mt ] { return charset[ char_grab( mt ) ]; } );

    return str;
}

void show_usage() {
    std::cerr << "Usage: window_title_spoofer [mandatory switches] [options]\n\n"
        "window_title_spoofer is a tool to locate all currently open top-level windows and spoof them to a user chosen string.\n\n"
        "Mandatory switches:\n\n"
        "You must specify either -p or -t <list of parameters seperated by semicolons>\n"
        "-p\n"
        "       Searches windows associated with the list of PIDs given\n\n"
        "-t\n"
        "       Searches windows containing strings in the list given\n\n"
        "-s <string to spoof window to>\n"
        "       Sets a custom string to set the found windows title to\n\n"
        "Options:\n\n"
        "-v\n"
        "       Limits the window search to only windows that are considere visible\n"
        "-x\n"
        "       Randomizes the window title of the tool\n\n"
        "Examples:\n\n"
        "Randomize the tool's window, search windows associated with the PIDs: 1390, 101 and 203 and are visible only and replace them with \"123\":\n\n"
        "       window_title_changer -p 1390;101;203 -s \"123\" -v -x \n\n"
        "Search all windows that contain \"IDA\", \"Cheat\" or \"Thread\" and replace them with \"spoofed\":\n\n"
        "       window_title_changer -t \"IDA;Cheat;Thread\" -s \"spoofed\"\n\n";
}

BOOL WINAPI windows_enum_callback( HWND hwnd, LPARAM lparam ) {

    if( hwnd == nullptr )
        return TRUE;

    options_t &options = *reinterpret_cast<options_t*>(lparam);

    auto grab_window_data = [ options ]( HWND hwnd ) {
        std::pair<std::string, std::string> pair;
        if( !options.only_check_visible_windows || ( options.only_check_visible_windows && IsWindowVisible( hwnd ) ) ) {
            // grab the window class name and window title
            std::array<char, 256> window_class_name {};
            if( GetClassName( hwnd, window_class_name.data(), window_class_name.size() ) ) {
                std::array<char, 256> window_title {};
                if( GetWindowText( hwnd, window_title.data(), window_title.size() ) ) {
                    pair = { window_class_name.data(), window_title.data() };
                }
            }
        }
        return pair;
    };

    if( options.using_pids ) {
        DWORD pid = 0;
        DWORD thread_pid = GetWindowThreadProcessId( hwnd, &pid );

        for( const auto& pid_str : options.window_data_to_search ) {
            if( static_cast<DWORD>( std::stoi( pid_str ) ) == pid ) {
                auto pair = grab_window_data( hwnd );
                if( !pair.first.empty() && !pair.second.empty() ) {
                    options.windows.push_back( pair );
                }
            }
        }
    }
    else if( options.using_titles ) {
        auto pair = grab_window_data( hwnd );
        for( const auto& title : options.window_data_to_search ) {
            if( pair.second.find( title ) != std::string::npos ) {
                options.windows.push_back( pair );
            }
        }
    }
    return TRUE;
}

int main( int argc, char* argv[] ) {
    if( argc < 2 ) {
        show_usage();
        return 1;
    }

    // todo: find a cleaner way of doing this in modern C++ because my god, this is bad.
    std::stringstream msg_stream {};

    options_t options {};

    // hacky and quick way to parse cmd line arguments without external libraries/overhead
    // since our usage order is specific: [mandatory] [options]

    for( int i = 1; i < argc; ++i ) {
        if( argv[ i ] == nullptr )
            break;

        // search via pid list
        if( std::string( argv[ i ] ).compare( "-p" ) == 0 && argv[ i + 1 ] != nullptr ) {
            options.using_pids = true;

            options.window_data_to_search = parse_semicolon_seperated_string( std::string( argv[ ++i ] ) );
            continue;
        }
        // search via title list
        else if( std::string( argv[ i ] ).compare( "-t" ) == 0 && argv[ i + 1 ] != nullptr ) {
            options.using_titles = true;

            options.window_data_to_search = parse_semicolon_seperated_string( std::string( argv[ ++i ] ) );
            continue;
        }
        // grab the spoofed string
        else if( std::string( argv[ i ] ).compare( "-s" ) == 0 && argv[ i + 1 ] != nullptr ) {
            options.spoof_string = argv[ ++i ];
            continue;
        }

        // grab booleans
        else if( std::string( argv[ i ] ).compare( "-v" ) == 0 ) {
            options.only_check_visible_windows = true;
        }
        else if( std::string( argv[ i ] ).compare( "-x" ) == 0 ) {
            if( SetWindowText( GetConsoleWindow(), get_random_string().c_str() ) == 0 ) {
                msg_stream << "Unable to set the tool's window title to a random string.";
                print_status_msg( status_type_t::FAILURE_WITH_ERROR_CODE, msg_stream );
            }
        }
    }

    // parse error checks
    if( options.using_titles && options.using_pids ) {
        msg_stream << "Please only search windows via strings or PIDs. Check the usage prompt for an example on how to use this tool.";
        print_status_msg( status_type_t::FAILURE, msg_stream );
        show_usage();
        return 1;
    }

    if( options.spoof_string.empty() ) {
        msg_stream << "Please specify a string to spoof all windows found to. Check the usage prompt for an example on how to use this tool.";
        print_status_msg( status_type_t::FAILURE, msg_stream );
        show_usage();
        return 1;
    }

    // obtain window data
    if( EnumWindows( windows_enum_callback, reinterpret_cast<LPARAM>( &options ) ) == 0 ) {
        msg_stream << "Unable to enumerate windows via ";
        if( options.using_titles ) {
            msg_stream << "searching window titles";
        }
        else if( options.using_pids ) {
            msg_stream << "searching process ids";
        }
        print_status_msg( status_type_t::FAILURE_WITH_ERROR_CODE, msg_stream );
        return 1;
    }

    // verify window data
    if( !options.windows.empty() ) {
        msg_stream << "Detected " << options.windows.size() << " windows to spoof.";
        print_status_msg( status_type_t::SUCCESS, msg_stream );
    } 
    else {
        msg_stream << "Unable to find windows based on the specifications given.";
        print_status_msg( status_type_t::FAILURE, msg_stream );
        return 1;
    }

    // spoof windows and keep track of success rate
    int spoofed_count = 0;
    for( const auto& window : options.windows ) {
        HWND hwnd = FindWindowA( window.first.c_str(), window.second.c_str() );
        if( hwnd == nullptr ) {
            msg_stream << "Can't find the window: \"" << window.first.c_str() << "\"";
            print_status_msg( status_type_t::FAILURE_WITH_ERROR_CODE, msg_stream );
            continue;
        }

        if( SetWindowTextA( hwnd, options.spoof_string.c_str() ) != 0 ) {
            ++spoofed_count;
        }
        else {
            msg_stream << "Can't set the window: \"" << window.first.c_str() << "\" 's text.";
            print_status_msg( status_type_t::FAILURE_WITH_ERROR_CODE, msg_stream );
        }
    }

    // check success rate
    if( spoofed_count == options.windows.size() ) {
        msg_stream << "Successfully spoofed all windows.";
        print_status_msg( status_type_t::SUCCESS, msg_stream );
    } 
    else {
        msg_stream << "Unsuccessfully spoofed all windows. Only spoofed " << spoofed_count << " out of " << options.windows.size();
        print_status_msg( status_type_t::FAILURE, msg_stream );
        return 1;
    }

    return 0;
}