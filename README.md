# DCFileWatcher
 DCFileWatcher is a filter written in C++ (on Windows platform) that accepts the hot folder path and backup folder path, backing up any file that is created or modified in the chosen folder.
 
 Application can delete file if the new file name has "delete_" prefix.
 
 Application can delete file at specific time if the new file name has "delete_2021-05-24 05-06-15" prefix.
 
# How to build
 Application is created, modified and tested with Visual Studio 2017. All required project files are included in the repository. 
 Just open DCFileWatcher.vcxproj and compile.
 Note: project requires to use /std:c++17 flag. Keep that in mind if other compilers will be used.
 
# How to run
 DCFileWatcher accepts two parameters: a path to watch and a path to create backups.
 ```
 DCFileWatcher.exe C:\FolderToWatch D:\Backup
 ``` 
 If DCFileWatcher is executed without any parameters - previously used non-empty parameters will be used instead.
 Previously entered parameters are saved in the ini file.
 
 DCFileWatcher starts and waits for user input on log file filtering. Just enter filter text and press Enter key. If filter text is empty - all log file will be printed to the screen.
 
# How to exit
 Enter ":q" to the filter and press enter. Application will be closed.
 
 # Main Window with filter "22:25:23" 
 ![Imgur Image](https://i.imgur.com/WtAKFXS.png)
