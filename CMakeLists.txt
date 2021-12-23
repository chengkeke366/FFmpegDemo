cmake_minimum_required(VERSION 3.15)
project(FFmpeg_Demo)

set(CMAKE_CXX_STANDARD 11)
message("FFMPEG_HOME:$ENV{FFmpegHome}")
include_directories($ENV{FFmpegHome}/include)
link_directories($ENV{FFmpegHome}/lib)

#avio_reading.exe
add_executable(avio_reading "avio_reading.cpp" "global.h")
target_link_libraries(avio_reading avcodec avformat avutil)

#hw_decode.exe
add_executable(hw_decode "hw_decode.cpp" "global.h" )
target_link_libraries(hw_decode avcodec avformat avutil)

#qsvdec.exe
add_executable(qsvdec "qsvdec.cpp")
target_link_libraries(qsvdec avcodec avformat avutil)

#decode_audio.exe
add_executable(decode_audio "decode_audio.cpp" "global.h")
target_link_libraries(decode_audio avcodec avformat avutil)

#resampling_audio
add_executable(resampling_audio "resampling_audio.cpp")
target_link_libraries(resampling_audio avcodecd avformatd avutild swresampled)

#scaling_video
add_executable(scaling_video "scaling_video.cpp")
target_link_libraries(scaling_video avcodec avformat avutil swscale)


#transcode_aac
add_executable(transcode_aac "transcode_aac.cpp")
target_link_libraries(transcode_aac avcodecd avformatd avutild swresampled)

#transcoding

add_executable(transcoding "transcoding.cpp")
target_link_libraries(transcoding avcodecd avformatd avutild swresampled avfilterd)
