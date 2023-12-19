###!
 # @file rfid_and_lcd_with_spotify.py
 # @author Jordan Fraser, Nina Okubo
 # @date August 18, 2023
 # @brief ECE Lab 4, Part C. Making API requests to Spotify
 # @detail University of Washington ECE 474
 # Acknowledgments: Serial communication was inspired by the following GitHub by Rahul.S
 # GitHub: https://github.com/xanthium-enterprises/CrossPlatform-Serial-Port-Programming-using-Python-and-PySerial/tree/master
 # 
 # This program plays the song/album on Spotify sent by Serial Communication, and also sends the current song being played
 # on Spotify to the Serial
###

from dotenv import load_dotenv
import os
import serial
import time
# spotify imports:
import spotipy
from spotipy.oauth2 import SpotifyOAuth

load_dotenv()

CLIENT_ID = os.getenv("client_id") ## Spotify CLIENT_ID
CLIENT_SECRET = os.getenv("client_secret") ## Spotify CLIENT_SECRET

sp = spotipy.Spotify(auth_manager=SpotifyOAuth(client_id=CLIENT_ID,
                                               client_secret=CLIENT_SECRET,
                                               redirect_uri="http://localhost:3000",
                                               scope="user-read-playback-state,user-modify-playback-state")) ## Spotify object

SerialObj = serial.Serial('COM4') ## Serial Object for serial communication

def start_album(album_title: str) -> None:
    ## 
    # @author Jordan Fraser, Nina Okubo
    # @brief Plays the given album code on Spotify
    # @param album_title The Spotify album code as a string to play on Spotify
    # 
    # Plays the given album code on Spotify
    # Acknowledgements: The Spotipy Library Documentation
    # @see read_value()
    sp.start_playback(context_uri=album_title)


def start_song(song_title: str) -> None:
    ##
    # @author Jordan Fraser, Nina Okubo
    # @brief Plays the given song/track code on Spotify
    # @param song_title The Spotify song code as a string to play on Spotify
    # 
    # Plays the given song on Spotify
    # Acknowledgements: The Spotipy Library Documentation
    # @see read_value()
    sp.start_playback(uris=[song_title])


def read_value() -> None:
    ##
    # @author Jordan Fraser, Nina Okubo
    # @brief Plays the given song/track code on Spotify
    # 
    # Reads the value from Serial, and plays the given album/song.
    # Acknowledgements: Serial communication inspired by the following github repo: 
    # https://github.com/xanthium-enterprises/CrossPlatform-Serial-Port-Programming-using-Python-and-PySerial/blob/master/_1_Python3_Codes/_5_PySerial-Receive_String.py
    # @see main(), start_album(), start_song()
    time.sleep(3)
    SerialObj.timeout = 3 # set the Read Timeout (read data after 3 seconds)
    ReceivedString = SerialObj.readline() #readline reads a string terminated by \n (reads data from the Serial Port) (must have \n from Arduino)
    no_n_and_r = ReceivedString.rstrip().decode() # rstrip gets rid of \r\n, and .decode makes it from bytes to string
    start_of_call = no_n_and_r[0:13] # returns spotify:album
    if (start_of_call == "spotify:album"):
        start_album(no_n_and_r)
    elif (start_of_call == "spotify:track"):
        start_song(no_n_and_r)

def get_currently_playing_song(prev_song_info, prev_song_str):
    ##
    # @author Jordan Fraser, Nina Okubo
    # @brief Sends the new song information to serial, if the song playing on Spotify has changed
    # @param prev_song_info Dictionary with the info about the most previous song that was sent to Serial
    # @param prev_song_str The most recent song string that was sent to Serial
    # 
    # If the current song playing on Spotify has changed since the most recent song info sent to Serial,
    # then we get the information about the current song playing, and send the new song's title and artist
    # to Serial.
    # Acknowledgements: Serial communication inspired by the following github repo: 
    # https://github.com/xanthium-enterprises/CrossPlatform-Serial-Port-Programming-using-Python-and-PySerial/blob/master/_1_Python3_Codes/_2_PySerial_Transmit_2.py
    # @see main()
    song_info = sp.current_user_playing_track()
    if ((song_info != prev_song_info)):
        try:
            song_artist = song_info["item"]["artists"][0]["name"] # artist name
            song_title = song_info["item"]["name"] # song title
            result_str = "Song: " + song_title + " by " + song_artist
        except:
            print("error in getting the song title")
        if (result_str != prev_song_str):
            song_send_str = "Song: " + song_title
            SerialObj.write(bytes(song_send_str, 'utf-8'))
            time.sleep(1.1)
            artist_send_str = "By: " + song_artist
            SerialObj.write(bytes(artist_send_str, 'utf-8'))
            prev_song_str = result_str
        prev_song_info = song_info


def main():
    ## Main program entry.
    # @author Jordan Fraser, Nina Okubo
    # @brief Main method where loop is run to continously check song/album values being sent and played
    # 
    # Continuously checks if a card value has been sent, or if the song has changed,
    # communicating with the Serial
    prev_song_info = None
    prev_song_str = ""
    while True:
        try:
            read_value()
            time.sleep(0.2)
            get_currently_playing_song(prev_song_info, prev_song_str)
        except:
            print("an error has occurred")
    SerialObj.close()
    
if __name__ == '__main__':
    main()