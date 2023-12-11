
#!!!!note: tensorflow only currently supported for  < Python 3.12.0

#importing libraries
import pandas as pd 
import tensorflow as tf
import os
import numpy as np
import matplotlib.pyplot as plt
from collections import namedtuple

from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import *
from tensorflow.keras.callbacks import ModelCheckpoint
from tensorflow.keras.losses import MeanSquaredError
from tensorflow.keras.metrics import RootMeanSquaredError
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.models import load_model


'''
Need to make the following changes:

Change read("test.csv") to loop through all training sets in directory training_sets, use this as X (to train modelN) - 
that is for each station - train a different model on its corresponding training set.
 
Modify get_predictions() as needed to predict sequence on the appropriate model.
'''




df = pd.read_csv('mpi_roof.csv', encoding='ISO-8859-1') #reading in temperature data from a random dataset in order to train model
df = df[5::6] #this removes every element in the dataset that is not recorded on the hour

df.index = pd.to_datetime(df['Date Time'], format='%d.%m.%Y %H:%M:%S') #converts column of data into datetime objects which is easier for pandas to work with
temp = df['T (degC)'] #setting temp equal to the temp values 

#Below function takes in the dataframe and window size (length of input sequences) and returns list of X and Y values which contain the input sequences for the model
def df_to_X_y(df, window_size = 4):
    df_np = df.to_numpy()
    X = []
    y = []
    for i in range(len(df_np)-window_size):
        row = [[a] for a in df_np[i:i+4]]
        X.append(row)
        label = df_np[i+4]
        y.append(label)
    return np.array(X), np.array(y)

window_size = 4
X, y = df_to_X_y(temp, window_size)

X_train, y_train = X[:2000], y[:2000]
X_test, y_test = X[2000:2500], y[2000:25000]
X_val, y_val = X[2500:], y[2500:]

model1 = Sequential()
model1.add(InputLayer((4,1)))
model1.add(LSTM(64))
model1.add(Dense(8, 'relu'))
model1.add(Dense(1, 'linear'))
#model1.summary()

cp = ModelCheckpoint('model1/', save_best_only=True) #saves the best model 
model1.compile(loss=MeanSquaredError(), optimizer = Adam(learning_rate=0.0001), metrics=[RootMeanSquaredError()]) #

#below is commented out as we have already fit model so no need to fit model every time you want a prediction
#model1.fit(X_train, y_train, validation_data=(X_val, y_val), epochs=100, callbacks=[cp])

model1 = load_model('model1/')


df1 = pd.read_csv('training_sets/station1.csv', encoding='UTF-8') #assuming their is only one station currently but easily expendable 
df1['Date Time'] = df1.year + ' ' + df1.month + ' ' + df1.day + ' ' + df1.hour + ' ' + df1.minute + ' ' df1.second
df1['Date Time'] = pd.to_datetime(df1['Date Time'], format = '%Y %M %d %h %m %s')
new_temp = df1['temperature']
row_count = len(df1)


# Define the number of future steps you want to predict
NUM_OF_FUTURE_STEPS = 1

# Start with the last `window_size` actual temperatures as the initial sequence
last_sequence = new_temp[-window_size:].to_numpy()

future_temperatures = []
#gets the number of stations need to return predictions for
NUM_OF_STATIONS = 1 #len([f for f in os.listdir("training_sets") if os.path.isfile(os.path.join("training_sets", f))])

StationPrediction = namedtuple('StationPrediction', ['id', 'predictions'])

def get_predictions1():
    future_predictions = []
    for i in range(NUM_OF_STATIONS):
        # Recursively predict the next temperature
        for _ in range(NUM_OF_FUTURE_STEPS):
            # Reshape the last sequence for prediction
            sequence_for_prediction = last_sequence.reshape((1, window_size, 1))
            
            # Predict the next temperature
            next_temperature_pred = model1.predict(sequence_for_prediction).flatten()[0]
            
            # Append the prediction to the list of future temperatures
            future_temperatures.append(next_temperature_pred)
            
            # Update the sequence with the predicted value
            last_sequence = np.append(last_sequence[1:], next_temperature_pred)

        #return result
        future_predictions.append(StationPrediction(i, future_temperatures))

        
        
    return future_predictions

def get_predictions2():
    future_predictions = []
    for i in range(NUM_OF_STATIONS):
        # Recursively predict the next temperature
        for _ in range(NUM_OF_FUTURE_STEPS):
            # Reshape the last sequence for prediction
            sequence_for_prediction = last_sequence.reshape((1, window_size, 1))
            
            # Predict the next temperature
            next_temperature_pred = model2.predict(sequence_for_prediction).flatten()[0]
            
            # Append the prediction to the list of future temperatures
            future_temperatures.append(next_temperature_pred)
            
            # Update the sequence with the predicted value
            last_sequence = np.append(last_sequence[1:], next_temperature_pred)

        #return result
        future_predictions.append(StationPrediction(i, future_temperatures))


if row_count > 2000: #if the data from the weather stations is large enough train a new model with that data and use it to predict future temperatures. 
    model2 = Sequential()
    model2.add(InputLayer((4,1)))
    model2.add(LSTM(64))
    model2.add(Dense(8, 'relu'))
    model2.add(Dense(1, 'linear'))

    X, y = df_to_X_y(new_temp, window_size)
    X_train, y_train = X[:1500], y[:1500]
    X_val, y_val = X[1500:], y[1500:]

    cp1 = ModelCheckpoint('model2/', save_best_only=True) #saves the best model 
    model2.compile(loss=MeanSquaredError(), optimizer = Adam(learning_rate=0.0001), metrics=[RootMeanSquaredError()]) 
    model2.fit(X_train, y_train, validation_data=(X_val, y_val), epochs=100, callbacks=[cp1])
    
    model2 = load_model('model2/')

    get_predictions2()
else: #otherwise use exisiting model1 which is data from a weather station in Germany to predict the future temperatures. 
    get_predictions1()


#modified to return a namedtuple containing each station ID and its future temperatures.

def print_predictions ():
    # Display the future temperatures
    print("Future temperature predictions:")
    for i, temp in enumerate(future_temperatures, 1):
        print(f"Next temperature {i}: {temp}")
