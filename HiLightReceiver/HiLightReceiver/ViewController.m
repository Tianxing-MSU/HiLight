//
//  ViewController.m
//  SlowMotionVideoRecorder
//
//  Created by shuichi on 12/17/13.
//  Copyright (c) 2013 Shuichi Tsutsumi. All rights reserved.
//

#import "ViewController.h"
#import "SVProgressHUD.h"
#import "AVCaptureManager.h"
#import <AssetsLibrary/AssetsLibrary.h>

#import "CAIndicatorView.h"

@interface ViewController ()
<AVCaptureManagerDelegate>
{
    BOOL isNeededToSave;
    double current_timestamp;
    double previous_timestamp;
    BOOL start;
    bool start_recording;
    HiLight_results *output_results;
    int screen_points[9];
    int print_log_line_number;
}
@property (nonatomic, strong) AVCaptureManager *captureManager;
@property (nonatomic, assign) NSTimer *fps_timer;
@property (nonatomic, strong) UIImage *recStartImage;
@property (nonatomic, strong) UIImage *recStopImage;
@property (nonatomic, strong) UIImage *outerImage1;
@property (nonatomic, strong) UIImage *outerImage2;
@property (strong, nonatomic) IBOutlet CAIndicatorView *indicatorView;

@property (nonatomic, weak) IBOutlet UILabel *statusLabel;
@property (weak, nonatomic) IBOutlet UILabel *Labelstatus;
@property (nonatomic, weak) IBOutlet UISegmentedControl *fpsControl;
@property (nonatomic, weak) IBOutlet UIButton *recBtn;
@property (nonatomic, weak) IBOutlet UIButton *screen_detector;
@property (nonatomic, weak) IBOutlet UIImageView *outerImageView;
@property (nonatomic, assign) NSTimer *screen_detector_timer;
@property (nonatomic, retain) UIAlertView *alert;

@property (weak, nonatomic) IBOutlet UILabel *Final_text1;
@property (weak, nonatomic) IBOutlet UILabel *Final_text2;
@property (weak, nonatomic) IBOutlet UILabel *Final_text3;
@property (weak, nonatomic) IBOutlet UILabel *Final_text4;

@property (weak, nonatomic) IBOutlet UIView *background1;
@property (weak, nonatomic) IBOutlet UIView *background2;
@property (weak, nonatomic) IBOutlet UIView *background3;

@end


@implementation ViewController
@synthesize fpsLabel = _fpsLabel;
@synthesize indicatorView = _indicatorView;
- (void)viewDidLoad
{
    [super viewDidLoad];
    
    _background1.backgroundColor = [UIColor clearColor];
    _background2.backgroundColor = [UIColor clearColor];
    _background3.backgroundColor = [UIColor clearColor];
    
    start = false;
    start_recording = false;
    
    //Create HiLight results struct
    output_results = [[HiLight_results alloc] init];
    print_log_line_number = 0;
    
    self.captureManager = [[AVCaptureManager alloc] initWithPreviewView:self.view];
    self.captureManager.delegate = self;
    
    UITapGestureRecognizer *tapGesture = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                                 action:@selector(handleDoubleTap:)];
    tapGesture.numberOfTapsRequired = 2;
    [self.view addGestureRecognizer:tapGesture];
    
    
    // Setup images for the Shutter Button
    UIImage *image;
    image = [UIImage imageNamed:@"ShutterButtonStart"];
    self.recStartImage = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self.recBtn setImage:self.recStartImage
                 forState:UIControlStateNormal];
    
    image = [UIImage imageNamed:@"ShutterButtonStop"];
    self.recStopImage = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

    [self.recBtn setTintColor:[UIColor colorWithRed:245./255.
                                              green:51./255.
                                               blue:51./255.
                                              alpha:1.0]];
    self.outerImage1 = [UIImage imageNamed:@"outer1"];
    self.outerImage2 = [UIImage imageNamed:@"outer2"];
    self.outerImageView.image = self.outerImage1;
    
    self.captureOutput = [[AVCaptureVideoDataOutput alloc] init];
    self.captureOutput.alwaysDiscardsLateVideoFrames = YES;
    
    // Set the video output to store frame in BGRA (It is supposed to be faster)
    NSString* key = (NSString*)kCVPixelBufferPixelFormatTypeKey;
    NSNumber* value = [NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA];
    NSDictionary* videoSettings = [NSDictionary dictionaryWithObject:value forKey:key];
    [self.captureOutput setVideoSettings:videoSettings];
    
    /*We create a serial queue to handle the processing of our frames*/
    dispatch_queue_t queue;
    queue = dispatch_queue_create("FrameQueue", NULL);
    [self.captureOutput setSampleBufferDelegate:self queue:queue];
    [self.captureManager.captureSession addOutput:self.captureOutput];
    [self.captureManager.captureSession startRunning];
    
    //start FPS calculator
    self.fps_timer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                  target:self
                                                selector:@selector(FPSHandler:)
                                                userInfo:nil
                                                 repeats:YES];
    
    self.screen_detector_timer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:_indicatorView selector:@selector(setNeedsDisplay) userInfo:nil repeats:YES];
}

-(void) viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:YES];
    
    self.captureManager.previewLayer.frame = self.view.bounds;
    
    //Get Preview Layer connection
    AVCaptureConnection *previewLayerConnection=self.captureManager.previewLayer.connection;
    
    if ([previewLayerConnection isVideoOrientationSupported])
        previewLayerConnection.videoOrientation = UIInterfaceOrientationLandscapeRight;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    current_timestamp = [[NSDate date] timeIntervalSince1970];
    if (!start) {
        start = true;
        previous_timestamp = [[NSDate date] timeIntervalSince1970];
        _fps = 0;
    }else{
        _fps = 1/(current_timestamp - previous_timestamp);
        previous_timestamp = current_timestamp;
    }
    
    _fpsLabel.text = [NSString stringWithFormat:@"%.4f FPS", _fps];
    output_results = [self.captureManager didSampleBuffer:(CMSampleBufferRef)sampleBuffer :[self videoOrientation] :current_timestamp] ;

    if(output_results->screen_position[0]>0){
        _indicatorView = [[CAIndicatorView alloc] init];
        _indicatorView.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin|UIViewAutoresizingFlexibleLeftMargin|UIViewAutoresizingFlexibleRightMargin|UIViewAutoresizingFlexibleTopMargin;
        _indicatorView.center = self.view.center;
        [self.view addSubview:_indicatorView];
        screen_points[0] = (int)output_results->screen_position[0];
        screen_points[1] = (int)output_results->screen_position[1];
        screen_points[2] = (int)output_results->screen_position[2];
        screen_points[3] = (int)output_results->screen_position[3];
        screen_points[4] = (int)output_results->screen_position[4];
        screen_points[5] = (int)output_results->screen_position[5];
        screen_points[6] = (int)output_results->screen_position[6];
        screen_points[7] = (int)output_results->screen_position[7];
        screen_points[8] = (int)output_results->screen_position[8];
        [_indicatorView setScreen:screen_points];
        self.indicatorView.transform = CGAffineTransformMakeRotation( ( 180 * M_PI ) / 360 );
    }
    
    //update Data transmission status
    if(output_results->denoise_check){
        dispatch_async(dispatch_get_main_queue(), ^(void){
            self.Labelstatus.text = [NSString stringWithFormat:@"Status: Detecting bits..."];
        });
    }
    
    if(output_results->first_bit_check){
        dispatch_async(dispatch_get_main_queue(), ^(void){
            self.Labelstatus.text = [NSString stringWithFormat:@"Status: Receving data..."];
        });
    }
    
    if(output_results.output_check){
        print_log_line_number++;
        
        //update received data message on the screen
        if (print_log_line_number == 1){
            dispatch_async(dispatch_get_main_queue(), ^(void){
                self.Labelstatus.text = [NSString stringWithFormat:@"Status: Data detected."];
                self.Final_text1.text = output_results.output_text;
                self.Final_text2.text = [NSString stringWithFormat:@""];
                self.Final_text3.text = [NSString stringWithFormat:@""];
                self.Final_text4.text = [NSString stringWithFormat:@""];
            });
        }else if ( print_log_line_number == 2){
            dispatch_async(dispatch_get_main_queue(), ^(void){
                self.Labelstatus.text = [NSString stringWithFormat:@"Status: Data detected."];
                self.Final_text2.text = output_results.output_text;
            });
        }else if ( print_log_line_number == 3){
            dispatch_async(dispatch_get_main_queue(), ^(void){
                self.Labelstatus.text = [NSString stringWithFormat:@"Status: Data detected."];
                self.Final_text3.text = output_results.output_text;
            });
        }else if ( print_log_line_number == 4){
            print_log_line_number = 0;
            dispatch_async(dispatch_get_main_queue(), ^(void){
                self.Labelstatus.text = [NSString stringWithFormat:@"Status: Data detected."];
                self.Final_text4.text = output_results.output_text;
            });
        }
    }
}

- (void)didPresentAlertView:(UIAlertView *)alertView
{
    // UIAlertView in landscape mode
    [UIView beginAnimations:@"" context:nil];
    [UIView setAnimationDuration:0.1];
    alertView.transform = CGAffineTransformRotate(alertView.transform, 3.14159/2);
    [UIView commitAnimations];
}

- (AVCaptureVideoOrientation) videoOrientation
{
    AVCaptureConnection * connection = [self.captureOutput connectionWithMediaType:AVMediaTypeVideo];
    if (connection)
        return [connection videoOrientation];

    NSLog(@"Warning  - cannot find AVCaptureConnection object");
    return AVCaptureVideoOrientationLandscapeRight;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

// =============================================================================
#pragma mark - Gesture Handler

- (void)handleDoubleTap:(UITapGestureRecognizer *)sender {

    [self.captureManager toggleContentsGravity];
}


// =============================================================================
#pragma mark - Private


- (void)saveRecordedFile:(NSURL *)recordedFile {
    
    [SVProgressHUD showWithStatus:@"Saving..."
                         maskType:SVProgressHUDMaskTypeGradient];
    
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(queue, ^{
        
        ALAssetsLibrary *assetLibrary = [[ALAssetsLibrary alloc] init];
        [assetLibrary writeVideoAtPathToSavedPhotosAlbum:recordedFile
                                         completionBlock:
         ^(NSURL *assetURL, NSError *error) {
             
             dispatch_async(dispatch_get_main_queue(), ^{
                 
                 [SVProgressHUD dismiss];
                 
                 NSString *title;
                 NSString *message;
                 
                 if (error != nil) {
                     
                     title = @"Failed to save video";
                     message = [error localizedDescription];
                 }
                 else {
                     title = @"Saved!";
                     message = nil;
                 }
                 
                 UIAlertView *alert = [[UIAlertView alloc] initWithTitle:title
                                                                 message:message
                                                                delegate:nil
                                                       cancelButtonTitle:@"OK"
                                                       otherButtonTitles:nil];
                 [alert show];
             });
         }];
    });
}

// =============================================================================
#pragma mark - Timer Handler

- (void)FPSHandler:(NSTimer *)timer {
    self.statusLabel.text = [NSString stringWithFormat:@"%.2f fps", _fps];
}

// =============================================================================
#pragma mark - AVCaptureManagerDeleagte

- (void)didFinishRecordingToOutputFileAtURL:(NSURL *)outputFileURL error:(NSError *)error {
    
    if (error) {
        NSLog(@"error:%@", error);
        return;
    }
    
    if (!isNeededToSave) {
        return;
    }
    
    [self saveRecordedFile:outputFileURL];
}

- (IBAction)detectScreenTapped:(id)sender {
    [self.captureManager detectScreenSwitch];
}

// =============================================================================
#pragma mark - IBAction

- (IBAction)recButtonTapped:(id)sender {
    
    // REC START
    if (!start_recording) {

        // change UI
        [self.recBtn setImage:self.recStopImage
                     forState:UIControlStateNormal];
        self.fpsControl.enabled = NO;
        self.screen_detector.enabled = NO;
        [self.screen_detector setTitleColor:[UIColor grayColor] forState:UIControlStateNormal];

        start_recording = true;
        [self.captureManager HiLight_start];
        self.Labelstatus.text = [NSString stringWithFormat:@"Status: Denosing..."];
        _background1.backgroundColor = [UIColor blackColor];
        _background2.backgroundColor = [UIColor blackColor];
        _background3.backgroundColor = [UIColor blackColor];
    }
    // REC STOP
    else {
        _background1.backgroundColor = [UIColor clearColor];
        _background2.backgroundColor = [UIColor clearColor];
        _background3.backgroundColor = [UIColor clearColor];
        isNeededToSave = YES;
        
        // change UI
        [self.recBtn setImage:self.recStartImage
                     forState:UIControlStateNormal];
        self.fpsControl.enabled = YES;
        self.screen_detector.enabled = YES;
        [self.screen_detector setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
        
        start_recording = false;
        [self.captureManager HiLight_end];
        self.Final_text1.text = [NSString stringWithFormat:@""];
        self.Final_text2.text = [NSString stringWithFormat:@""];
        self.Final_text3.text = [NSString stringWithFormat:@""];
        self.Final_text4.text = [NSString stringWithFormat:@""];
        
        print_log_line_number = 0;//reset print log line number
        
        self.Labelstatus.text = [NSString stringWithFormat:@"Status: Idle"];
    }
}

- (IBAction)retakeButtonTapped:(id)sender {
    
    isNeededToSave = NO;
    
    self.statusLabel.text = nil;
}

- (IBAction)fpsChanged:(UISegmentedControl *)sender {
    
    // Switch FPS for the current HiLight, it only receive data at 60 FPS frame rate
    
    CGFloat desiredFps = 0.0;;
    switch (self.fpsControl.selectedSegmentIndex) {
        case 0:
        default:
        {
            break;
        }
        case 1:
            desiredFps = 60.0;
            break;
        case 2:
            desiredFps = 120.0;
            break;
    }
    
    
    [SVProgressHUD showWithStatus:@"Switching..."
                         maskType:SVProgressHUDMaskTypeGradient];
        
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_async(queue, ^{
        
        if (desiredFps > 0.0) {
            [self.captureManager switchFormatWithDesiredFPS:desiredFps];
        }
        else {
            [self.captureManager resetFormat];
        }
        
        dispatch_async(dispatch_get_main_queue(), ^{

            if (desiredFps > 30.0) {
                self.outerImageView.image = self.outerImage2;
            }
            else {
                self.outerImageView.image = self.outerImage1;
            }
            [SVProgressHUD dismiss];
        });
    });
}

@end
