function Control_Resampling(root_path,signal_length,number_slice, output_dir)%

signal_type='Control';
signal_length=signal_length-1;

%***********************Create an Output Directory***********************%
try
    mkdir (output_dir);
catch exception
    throw(exception)
end

%************************Sub folders of signals************************%

filenames=dir(root_path);
dirflags=[filenames.isdir];
sub_dir=filenames(dirflags);
sub_dir_name=cell(1,numel(sub_dir) - 2);

for i =3:numel(sub_dir)
    sub_dir_name{i-2}=sub_dir(i).name; % Appending the name of sub directories
end


%****************Iterating through the sub-directories****************%
for num_sub =1:numel(sub_dir_name)
    
    disp(sub_dir_name{num_sub});
    sub_path=strjoin({root_path,'\',sub_dir_name{num_sub} },''); % Path of subdirectories
    sub_list=dir(sub_path); % list of all the files in subdirectories
    
    resampling_output_path=strjoin({output_dir,'\',signal_type,'\'},''); % Output path
    
    try
        
        mkdir (resampling_output_path);
        
        
    catch exception
        throw(exception)
    end
    
    %***************Iterating through files in sub-directory***********%
    for num_file=3:numel(sub_list)
        file_path=strjoin({sub_path,'\',sub_list(num_file).name },''); % Path of subdirectories
        
        
        x=load(file_path); % Loading file
        signal=double(x.Channel_1.Data); % Convert sample to double data type
        
        ipt=2500000;
        signal=signal(ipt:end);
        
        label=textscan(sub_dir_name{num_sub},'%s'); %Labelling of data
        label=label{1};
        
        disp(signal_type);
        ipt=1;
        for i=1:number_slice
            %***************Wavelet Packet Decomposition******************%
            
            ipt_end=ipt+signal_length;
            sub_signal=signal(ipt:ipt_end);
            
            
            sub_signal=transpose(sub_signal);
            sub_signal=num2cell(sub_signal);
            sub_signal(1,end+1)=cellstr(signal_type);
            sub_signal(1,end+1)=label;
            
            writecell(sub_signal,...
                strjoin({ resampling_output_path,'\', sub_dir_name{num_sub},'.csv'},''),...
                'WriteMode','append');
            
            
            
            ipt=ipt_end;
        end
        
    end
end
disp("Done with Resamping done")
end