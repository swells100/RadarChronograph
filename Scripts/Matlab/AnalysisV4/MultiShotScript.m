clear, clc, close all

% Constants
f0 = 24.125e9; % emitted radar frequency [Hz]
c = 3e8; % speed of light [m/s]
HPFCutoff = 50000; % [Hz]
LPFCutoff = 250000; % [Hz]

folder = "/Users/tj/Downloads/AR_Tests";
% filename = "Test01_Radar_AR15_01.dxd";
% filename = "Test02_Radar_AR15_02.dxd";
filename = "Test03_Radar_AR15_03.dxd";
% filename = "Test04_Radar_AR15_04.dxd";
filepath = fullfile(folder, filename);

% Read the file
DWReader = DWDataReader();

% 1. Open the file and keep the variable named 'dw' 
[success, dw] = DWReader.openFile(char(filepath));

% 2. Get the list of channels
channels = DWReader.getChannelList();

% 3. Extract the data (Assuming radar data is on the first channel)
[radar_data, timestamps] = DWReader.getScaledData(channels(1));

% 4. Build the "bridge" to trick the rest of the client's script
dw.global.DefaultSampleRate = dw.sample_rate; 

% Identify Spike events
threshholed = 

% Package our continuous data into the exact struct format they expect
dw.data(1).t = timestamps;
dw.data(1).I_DC = radar_data;

% Now define the variables for the rest of the script
NShots = length(dw.data);
SampleRate = dw.global.DefaultSampleRate; 

% Setup plots
fg = figure('Position', [100, 100, 1400, 350]); % Creates a wide figure window

ax1 = subplot(1, 4, 1);
xlabel("Time [ms]")
ylabel("Signal [mV]")

ax2 = subplot(1, 4, 2);
xlabel("Time [ms]")
ylabel("Signal [mV]")

ax3 = subplot(1, 4, 3);
xlabel("Time [ms]")
ylabel("Signal [mV]")

ax4 = subplot(1, 4, 4);
xlabel("Frequency [kHz]")
ylabel("FFT Amplitude")

for i = 1:NShots
    % Extract timestamps
    t = dw.data(i).t;
    t = t - t(1);
    
    % Extract signal data, filter, then plot.
    y = dw.data(i).I_DC;
    y = bandpass(y, [HPFCutoff, LPFCutoff], SampleRate);
    plot(ax1, t*1e3, y, '-', Color=[0 .4 0]);

    % Apply the split
end
NShots