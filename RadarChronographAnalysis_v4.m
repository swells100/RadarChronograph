clear, clc, close all

% Constants
f0 = 24.125e9; % emitted radar frequency [Hz]
c = 3e8; % speed of light [m/s]
HPFCutoff = 50000; % [Hz]
LPFCutoff = 250000; % [Hz]

% File Location
% filename = "2Channels_Gain50_Test1.dxd";
% filename = "NoGain_Straight_To_Dewesoft_Test1.dxd";
% filepath = fullfile(folder, filename);

% File Location
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

% Package our continuous data into the exact struct format they expect
dw.data(1).t = timestamps;
dw.data(1).I_DC = radar_data;

% Now define the variables for the rest of the script
NShots = length(dw.data); % Sets shots to 1 for the continuous dataset
SampleRate = dw.global.DefaultSampleRate; 

% Grab the events stored in the Dewesoft file
events = DWReader.getEvents();
NShots = length(events) / 3;
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

pos = 2;

for i = 1:NShots
    if i ~= 1
        pos = pos + 3;
    end
    disp("Processing Shot " + i)

    % Extract timestamps
    t = dw.data.t(i);
    t = t - t(1);
    
    % Extract signal data, filter, then plot.
    y = dw.data(i).I_DC;
    y = bandpass(y, [HPFCutoff, LPFCutoff], SampleRate);
    plot(ax1, t*1e3, y, '-', Color=[0 .4 0]);
    
    % Roughly bound the event.
    t_max = events(pos+1).time_stamp;
    t_min = events(pos-1).time_stamp;
    % t_max = t(k_max);
    % k1 = find(t >= t_max - 0.003, 1, "first");
    % k2 = find(t >= t_max + 0.006, 1,  "first");
    % ax1.XLim = [t(k1), t(k2)]*1e3;
    
    % Bound data above an amplitude threshold.
    % AmplitudeThreshold = min([0.5, 0.50*y_max]); % [mV]
    % k1 = find(y >= AmplitudeThreshold, 1, "first");
    % k2 = find(y >= AmplitudeThreshold, 1,  "last");
    % k2 = find(t >= t(k_max) + 0.0025, 1, "first");
    % xline(ax1, [t_min, t_max]*1e3, 'r-', LineWidth=1.0)
    % yline(ax1, AmplitudeThreshold, 'r--', LineWidth=1.0)
    
    % 2. Find the index in your continuous time array that is closest to the event
    [~, t_min_idx] = min(abs(dw.data(1).t - t_min));
    [~, t_max_idx] = min(abs(dw.data(1).t - t_max));

    t = t(t_min_idx:t_max_idx);
    y = y(t_min_idx:t_max_idx);
    
    % Apply a window.
    N = length(t);
    HalfN = floor(N/2);
    window = hann(N);
    y = y .* window;
    plot(ax2, t*1e3, y, '-', Color=[0 .4 0]);
    
    % % Plot a few of the cycles.
    % [~, k_max] = max(y);
    % t_max = t(k_max);
    % ax3.XLim = [t_max - 0.000030, t_max + 0.000030]*1e3;
    % plot(ax3, t*1e3, y, '.-', Color=[0 .4 0], MarkerSize=15);
    
    disp("FFT Started")
    % Perform the FTT, find the max value.
    y_spectral = abs(fft(y));
    FreqBins = [0:N-1]' * (SampleRate/N);
    [~, k_max] = max(y_spectral);
    BeatFreq(i,1) = FreqBins(k_max); % [Hz]
    ShotSpeed(i,1) = (BeatFreq(i)*c) / (2*f0); % [m/s]
    
    % Plot the FFT data.
    ax4.XLim = round([BeatFreq(i)/1e3 - 20, BeatFreq(i)/1e3 + 20], -1);
    plot(ax4, FreqBins(1:HalfN)/1e3, y_spectral(1:HalfN), 'b.-', MarkerSize=15);
    
    str = sprintf("Shot: %i\nFrequency: %.2f kHz\nSpeed: %.2f m/s", i, ...
        BeatFreq(i)/1e3, ShotSpeed(i));
    tb = text(ax4, 0.05, 0.95, str, 'Units', 'normalized', ...
         'VerticalAlignment', 'top', 'BackgroundColor', 'w');
    
    pause()
    cla(ax1);
    cla(ax2);
    cla(ax3);
    cla(ax4);
    delete(tb)
end

% Create a table for all shots.
tbl = table(BeatFreq/1e3, ShotSpeed);
tbl.Properties.VariableNames = ["BeatFrequency", "ShotSpeed"];
tbl.Properties.VariableUnits = ["kHz", "m/s"];
tbl

AverageShotSpeed = mean(ShotSpeed)
ShotStdDev = std(ShotSpeed)
ShotSpread = max(ShotSpeed) - min(ShotSpeed)