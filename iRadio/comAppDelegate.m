//
//  comAppDelegate.m
//  ABradio
//
//  Created by Jan Damek on /202/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "comAppDelegate.h"
#import "comFavoriteViewController.h"
#import "comWebViewController.h"
#import "comPlayerViewController.h"

//#import <AVFoundation/AVFoundation.h>
//#import <MediaPlayer/MPMusicPlayerController.h>

#import "AsyncImageView.h"
#import "GAITracker.h"
#import "GAI.h"

#import "Reachability.h"

#import "GADBannerView.h"

/******* Set your tracking ID here *******/
static NSString *const kTrackingId = @"UA-1904592-44";
static NSString *const kAllowTracking = @"allowTracking";

NSString *kOblibeneFile     = @"oblibene.dat";
NSString *kPoslouchaneFile  = @"poslouchane.dat";
NSString *kCategoryFile     = @"category.dat";
NSString *kURLXMLData       = @"http://m.abradio.cz/export/xmlsearch/?q=";
NSString *MyAdBanerIDiPhone = @"a14fd85404797af";
NSString *MyAdBanerIDiPad   = @"a14fd855405a384";

//#pragma mark -
//@interface comAppDelegate (Player)
//- (void)handleTimedMetadata:(AVMetadataItem*)timedMetadata;
//- (void)assetFailedToPrepareForPlayback:(NSError *)error;
//- (void)prepareToPlayAsset:(AVURLAsset *)asset withKeys:(NSArray *)requestedKeys;
//@end

@implementation comAppDelegate

@synthesize window = _window;
@synthesize table;
@synthesize radioList = _radioList;
@synthesize imageList = _imageList;
@synthesize description = _description;
@synthesize streamPlayer = _streamPlayer;
@synthesize dataPlayer = _dataPlayer;
@synthesize artSONG = _artSONG, artARTIST = _artARTIST;
@synthesize artImageView = _artImageView;
@synthesize oblibene = _oblibene;
@synthesize poslouchane = _poslouchane;
@synthesize isInSleep =_isInSleep;
@synthesize sleepTimeLeft = _sleepTimeLeft;
@synthesize activity = _activity;
@synthesize sleepLabel = _sleepLabel;
@synthesize cntBaner = _cntBaner;
@synthesize categoryAktual = _categoryAktual;

+ (GADBannerView*)getBanner
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    //kGADAdSizeBanner
    GADBannerView *_banner = [[GADBannerView alloc] initWithAdSize:kGADAdSizeSmartBannerPortrait];
    if (delegate.cntBaner==0){
        if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
            _banner.adUnitID = MyAdBanerIDiPhone;
        } else {
            _banner.adUnitID = MyAdBanerIDiPad;
        }}else {
            if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
                _banner.adUnitID =@"a14fb5734d516b3";
            } else {
                _banner.adUnitID = @"a14fb573e1ec2cd";
            }    }
    _banner.rootViewController = delegate.window.rootViewController;
    
    [_banner loadRequest:[GADRequest request]];
    
    delegate.cntBaner++;
    if (delegate.cntBaner==3){
        delegate.cntBaner=0;
    }
    return _banner;
}


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    __artIMG = @"";
    _artImages = [[NSMutableDictionary alloc] init];
    _artARTIST = @"";
    _artSONG = @"";
    _meta = @"";
    _isInSleep = NO;
    _sleepTimeLeft = 0;
    _description = nil;
    _sleepLabel = nil;
    _activity = nil;
    _artImageView = nil;
    _isInLoadXML = NO;
    
    UITabBar *tb = [UITabBar appearance];
    UINavigationBar *nb = [UINavigationBar appearance];
    UIBarButtonItem *bb = [UIBarButtonItem appearance];
    if (floor(NSFoundationVersionNumber) <= NSFoundationVersionNumber_iOS_6_1) {
        // Load resources for iOS 6.1 or earlier
        [tb setTintColor:[UIColor blackColor]];
        [nb setTintColor:[UIColor blackColor]];
        [bb setTintColor:[UIColor blackColor]];
    }else{
        [tb setTintColor:[UIColor whiteColor]];
        [nb setTintColor:[UIColor whiteColor]];
        [bb setTintColor:[UIColor whiteColor]];
    }
    _playerClass = [[comPlayer alloc]initWithURL:kURLXMLData];
    _playerClass.delegate = self;
    
    NSDictionary *userDefaultsDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                          @"20", @"listeners_max",
                                          nil];
    [[NSUserDefaults standardUserDefaults] registerDefaults:userDefaultsDefaults];
    
    _defaults = [NSUserDefaults standardUserDefaults];
       
    _cntBaner = 0;
    
    _imageList = [[NSMutableDictionary alloc] init];
    
    NSString *docsDir;
    NSArray *dirPaths;
    
    dirPaths = NSSearchPathForDirectoriesInDomains(
                                                   NSDocumentDirectory, NSUserDomainMask, YES);
    docsDir = [dirPaths objectAtIndex:0];
    NSString *pathOblibene = [[NSString alloc]
                              initWithString: [docsDir stringByAppendingPathComponent:
                                               kOblibeneFile]];
    NSString *pathPoslouchane = [[NSString alloc]
                                 initWithString: [docsDir stringByAppendingPathComponent:
                                                  kPoslouchaneFile]];
    
    NSString *pathCategory = [[NSString alloc]
                              initWithString: [docsDir stringByAppendingPathComponent:
                                               kCategoryFile]];
    _oblibene = [[NSMutableArray alloc] initWithContentsOfFile:pathOblibene];
    if (!_oblibene)
        _oblibene = [[NSMutableArray alloc] init];
    
    _poslouchane = [[NSMutableArray alloc] initWithContentsOfFile:pathPoslouchane];
    if (!_poslouchane)
        _poslouchane = [[NSMutableArray alloc] init];
    
    self.categoryAktual = [[NSMutableArray alloc] initWithContentsOfFile:pathCategory];
    if (!self.categoryAktual)
        self.categoryAktual = [[NSMutableArray alloc] init];
    _category = [[NSMutableArray alloc] initWithContentsOfFile:pathCategory];
    if (!_category)
        _category = [[NSMutableArray alloc] init];
    
    _radioAll = [[NSMutableArray alloc]init];
    for (int i=1;i<[self.categoryAktual count];i=i+2){
        NSMutableArray *radia = [self.categoryAktual objectAtIndex:i];
        for (int ii=0;ii<[radia count];ii=ii+2){
            [_radioAll addObject:[radia objectAtIndex:ii]];
            [_radioAll addObject:[radia objectAtIndex:ii+1]];
        }
    }
    
    [[UIApplication sharedApplication] setStatusBarHidden:NO];
    
    NSError *sessionError = nil;
    [[AVAudioSession sharedInstance] setDelegate:self];
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback error:&sessionError];
    UInt32 doChangeDefaultRoute = 1;
    AudioSessionSetProperty(kAudioSessionProperty_OverrideCategoryDefaultToSpeaker, sizeof(doChangeDefaultRoute), &doChangeDefaultRoute);
    
    [self.window makeKeyAndVisible];
    
    [_ani setActivityIndicatorViewStyle: UIActivityIndicatorViewStyleWhiteLarge];
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        _ani.center = CGPointMake(160, 260);
    }else{
        _ani.center = CGPointMake(384, 512);
        
    }
    _ani.layer.cornerRadius = 10;
    [_ani setBackgroundColor:[UIColor grayColor]];
    
    [self.window addSubview:_ani];
    _ani.hidesWhenStopped=NO;
    [_ani setHidden:YES];
    [_ani stopAnimating];
    
    [GAI sharedInstance].optOut =
    ![[NSUserDefaults standardUserDefaults] boolForKey:kAllowTracking];
    // Initialize Google Analytics with a 120-second dispatch interval. There is a
    // tradeoff between battery usage and timely dispatch.
    [GAI sharedInstance].dispatchInterval = 120;
    [GAI sharedInstance].trackUncaughtExceptions = YES;
    self.tracker = [[GAI sharedInstance] trackerWithName:@"ABRadio iOS Player"
                                              trackingId:kTrackingId];
    
    return YES;
}

-(NSString*) meta
{
    if (_meta){
        return _meta;
    }else {
        return nil;
    }
}

- (void) testTimer:(id)sender
{
    if (_sleepTimeLeft > 0)
    {
        _sleepTimeLeft -= 1;
        if (_sleepTimeLeft == 0) {
            [self playStop];
            
            if (_sleepTimer) {
                [_sleepTimer invalidate];
                _sleepTimer = nil;
            }
        }
    }
    
    if (_sleepLabel) {
        if (_sleepTimeLeft>0){
            _sleepLabel.text = [NSString stringWithFormat:@"%d min", _sleepTimeLeft];}
        else
            _sleepLabel.text = @"0 min";
    }
}

- (void)saveData
{
    [self cancelSearch];
    
    NSString *docsDir;
    NSArray *dirPaths;
    dirPaths = NSSearchPathForDirectoriesInDomains(
                                                   NSDocumentDirectory, NSUserDomainMask, YES);
    docsDir = [dirPaths objectAtIndex:0];
    NSString *pathOblibene = [[NSString alloc]
                              initWithString: [docsDir stringByAppendingPathComponent:
                                               kOblibeneFile]];
    NSString *pathPoslouchane = [[NSString alloc]
                                 initWithString: [docsDir stringByAppendingPathComponent:
                                                  kPoslouchaneFile]];
    
    NSString *pathCategorie = [[NSString alloc]
                               initWithString: [docsDir stringByAppendingPathComponent:
                                                kCategoryFile]];
    
    [_oblibene writeToFile:pathOblibene atomically:YES];
    [_poslouchane writeToFile:pathPoslouchane atomically:YES];
    
    if (_isInLoadXML) {
        [self.categoryAktual writeToFile:pathCategorie atomically:YES];
    }else {
        [_category writeToFile:pathCategorie atomically:YES];
    }
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [self saveData];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    if (_sleepTimeLeft>0)
    {
        if (!_sleepTimer) {
            _sleepTimer = [NSTimer scheduledTimerWithTimeInterval:60 target:self selector:@selector(testTimer:) userInfo:nil repeats:YES];
        }
    }else if (_sleepTimer) {
        [_sleepTimer invalidate];
        _sleepTimer = nil;
    }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    [self saveData];
}

- (NSString*) prg
{
    if (_dataPlayer) {
        NSString *program = [_dataPlayer valueForKey:@"program"];
        program = [program stringByReplacingOccurrencesOfString:@"\n" withString:@""];
        program = [program stringByReplacingOccurrencesOfString:@"\t" withString:@""];
        return program;
    } else {
        return nil;
    }
}

- (void)_showArt:(NSTimer *)timer
{
    NSString *praveHraje;
    if (![_artARTIST isEqualToString:@""]){
        praveHraje = [NSString stringWithFormat:@"%@ - %@",_artARTIST,_artSONG];
    }
    else {
        praveHraje = [NSString stringWithFormat:@"%@",[self meta]];
    }
    praveHraje = [praveHraje stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    praveHraje = [praveHraje stringByReplacingOccurrencesOfString:@"\t" withString:@""];
    
    if (_description)
        [_description setText:praveHraje];
    
    if (_artImageView){
        NSString *imgURL;
        if (![[self getArtImage:_artImageView]isEqualToString:@""]){
            imgURL = [self getArtImage:_artImageView];
            UIImage *image = [[[AsyncImageView alloc]init]getImage:imgURL list:_imageList imageView:_artImageView cell:nil];
            [_artImageView setImage:image];
        }else {
            [_artImageView setImage:[UIImage imageNamed:@"vodoznak"]];
        }
    }
}

-(void)reloadTable
{
    if (self.table){
        [self.table reloadData];
    }
}

- (void)parseXMLFileAtURL:(NSString *)URL
{
    if (!_isInLoadXML)
    {
        _isInLoadXML = YES;
        
        URL = [URL stringByReplacingOccurrencesOfString:@"\n" withString:@""];
        URL = [URL stringByReplacingOccurrencesOfString:@"\t" withString:@""];
        BOOL art=NO;
        
        if ([URL hasPrefix:kURLXMLData])
        {
            [_ani setHidden:NO];
            [_ani startAnimating];
            
            
            _cat = NO;
            _rad = NO;
            _stre = NO;
            
            _category = [[NSMutableArray alloc] init];
            _errorParsing=NO;
            
        }else {
            __artIMG = @"";
            _artImages = [[NSMutableDictionary alloc] init];
            _artSONG = @"";
            _artARTIST = @"";
            art=YES;
        }
        
        NSString *agentString = @"ABRadio/1.1 (iPhone/iPad) AppleWebKit/525.27.1 (KHTML, like Gecko) Version/3.2.1 Safari/525.27.1";
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                        [NSURL URLWithString:URL]];
        [request setValue:agentString forHTTPHeaderField:@"User-Agent"];
        
        NSString *reqSysVer = @"5.0";
        NSString *currSysVer = [[UIDevice currentDevice] systemVersion];
        if (!art && [currSysVer compare:reqSysVer options:NSNumericSearch] != NSOrderedAscending)
        {
            NSOperationQueue *queue = [[NSOperationQueue alloc] init];
            _isInLoadXML = YES;
            [NSURLConnection sendAsynchronousRequest:request queue:queue completionHandler:^(NSURLResponse *response, NSData *data, NSError *error)
             {
                 
                 if (!error && [data length] > 0)
                 {
                     _rssParser = [[NSXMLParser alloc] initWithData:data];
                     [_rssParser setDelegate:self];
                     
                     [_rssParser setShouldProcessNamespaces:NO];
                     [_rssParser setShouldReportNamespacePrefixes:NO];
                     [_rssParser setShouldResolveExternalEntities:NO];
                     
                     [_rssParser parse];
                     
                     self.categoryAktual = [_category copy];
                     
                     _radioAll = [[NSMutableArray alloc]init];
                     for (int i=1;i<[self.categoryAktual count];i=i+2){
                         NSMutableArray *radia = [self.categoryAktual objectAtIndex:i];
                         for (int ii=0;ii<[radia count];ii=ii+2){
                             [_radioAll addObject:[radia objectAtIndex:ii]];
                             [_radioAll addObject:[radia objectAtIndex:ii+1]];
                         }
                     }
                     
                     [self performSelectorOnMainThread:@selector(reloadTable) withObject:nil waitUntilDone:NO];
                     [self saveData];
                     NSDate *_firstDate = [NSDate date];
                     [_defaults setDouble:[_firstDate timeIntervalSinceReferenceDate] forKey:@"last_xml"];
                 }
                 else
                 {
                     NSLog(@"error loading XML");
                 }
                 
                 [_ani setHidden:YES];
                 [_ani stopAnimating];
                 [[_window layer]needsDisplay];
                 _isInLoadXML = NO;
             }];
        }else {
            NSData *data=[NSURLConnection sendSynchronousRequest:request returningResponse:nil error:nil];
            if (data && [data length]>0){
                _rssParser = [[NSXMLParser alloc] initWithData:data];
                [_rssParser setDelegate:self];
                
                [_rssParser setShouldProcessNamespaces:NO];
                [_rssParser setShouldReportNamespacePrefixes:NO];
                [_rssParser setShouldResolveExternalEntities:NO];
                
                [_rssParser parse];
                if (!art){
                    self.categoryAktual = [_category copy];
                    
                    _radioAll = [[NSMutableArray alloc]init];
                    for (int i=1;i<[self.categoryAktual count];i=i+2){
                        NSMutableArray *radia = [self.categoryAktual objectAtIndex:i];
                        for (int ii=0;ii<[radia count];ii=ii+2){
                            [_radioAll addObject:[radia objectAtIndex:ii]];
                            [_radioAll addObject:[radia objectAtIndex:ii+1]];
                        }
                    }
                    
                    if (self.table){
                        [self.table reloadData];
                    }
                    
                    [_ani setHidden:YES];
                    [_ani stopAnimating];
                    NSDate *_firstDate = [NSDate date];
                    [_defaults setDouble:[_firstDate timeIntervalSinceReferenceDate] forKey:@"last_xml"];
                }else {
                    __artIMG = [__artIMG stringByReplacingOccurrencesOfString:@"\n" withString:@""];
                    __artIMG = [__artIMG stringByReplacingOccurrencesOfString:@"\t" withString:@""];
                    _artSONG = [_artSONG stringByReplacingOccurrencesOfString:@"\n" withString:@""];
                    _artSONG = [_artSONG stringByReplacingOccurrencesOfString:@"\t" withString:@""];
                    _artARTIST = [_artARTIST stringByReplacingOccurrencesOfString:@"\n" withString:@""];
                    _artARTIST = [_artARTIST stringByReplacingOccurrencesOfString:@"\t" withString:@""];
                    [self _showArt:nil];
                }
            }
        }

    }else {
        [_ani setHidden:NO];
        [_ani startAnimating];
    }

}

- (void)parser:(NSXMLParser *)parser parseErrorOccurred:(NSError *)parseError {
    
    NSString *errorString = [NSString stringWithFormat:@"Error code %i", [parseError code]];
    NSLog(@"Error parsing XML: %@", errorString);
    
    
    _errorParsing=YES;
}

- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName attributes:(NSDictionary *)attributeDict
{
    _currentElement = [elementName copy];
    _ElementValue = [[NSMutableString alloc] init];
    if ([elementName isEqualToString:@"category"]) {
        _itemCat = [[NSMutableDictionary alloc] init];
        _radio = [[NSMutableArray alloc] init];
        _cat = YES;
    }else
        if ([elementName isEqualToString:@"radio"]) {
            _stream = [[NSMutableArray alloc] init];
            _itemRadio = [[NSMutableDictionary alloc] init];
            _rad = YES;
        }
        else
            if ([elementName isEqualToString:@"stream"]) {
                _itemStream = [[NSMutableDictionary alloc] init];
                _stre = YES;
            }
}

- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string{
    [_ElementValue appendString:string];
}

- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName
{
    if ([elementName isEqualToString:@"artist"])
    {
        _artARTIST = _ElementValue;
    }else if ([elementName isEqualToString:@"song"]) {
        _artSONG = _ElementValue;
    }else if ([elementName hasPrefix:@"image_"]) {
        __artIMG = _ElementValue;
        [_artImages setValue:__artIMG forKey:elementName];
    }
    
    if ([elementName isEqualToString:@"category"]) {
        [_category addObject:[_itemCat copy]];
        [_category addObject:[_radio copy]];
        _cat = NO;
    }else
        if ([elementName isEqualToString:@"radio"]) {
            [_radio addObject:[_itemRadio copy]];
            [_radio addObject:[_stream copy]];
            _rad = NO;
        }else
            if ([elementName isEqualToString:@"stream"]) {
                [_stream addObject:[_itemStream copy]];
                _stre = NO;
            }
    
    if (_stre) {
        [_itemStream setObject:_ElementValue forKey:elementName];
    }else
        if (_rad) {
            [_itemRadio setObject:_ElementValue forKey:elementName];
        }else
        {
            [_itemCat setObject:_ElementValue forKey:elementName];
        }
    
}

- (void)parserDidEndDocument:(NSXMLParser *)parser
{
    
    if (_errorParsing == NO)
    {
        NSLog(@"XML processing done!");
    } else {
        NSLog(@"Error occurred during XML processing");
    }
    
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    /*
     Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
     */
    NSDate* secondDate = [NSDate date];
    
    double _firstDate = [_defaults doubleForKey:@"last_xml"];
    
    NSTimeInterval timeDifference = [secondDate timeIntervalSinceReferenceDate];
    
    double Interval = timeDifference-_firstDate;
    if (Interval>60)
        [self performSelectorOnMainThread:@selector(parseXMLCategorie) withObject:nil waitUntilDone:NO];
}

-(void) parseXMLCategorie
{
    [self parseXMLFileAtURL:kURLXMLData];
}

-(void) doArtwork
{
    // provedeni parseru pro artwork k prehravaci
    NSString *artworkURL = [_dataPlayer valueForKey:@"artwork"];
    artworkURL = [artworkURL stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    artworkURL = [artworkURL stringByReplacingOccurrencesOfString:@"\t" withString:@""];
    if (![artworkURL isEqualToString:@""]){
        [self parseXMLFileAtURL:artworkURL];
        
    }
}

#pragma mark Oblibene

- (BOOL)isInOblibene
{
    BOOL ret = NO;
    for (NSString *line in _oblibene) {
        if ([line isEqual:[_dataPlayer valueForKey:@"id"]]){
            ret = YES;
            break;
        }
    }
    return ret;
}

- (void)togleInOblibene
{
    BOOL isIn = NO;
    for (int i=0;i<[_oblibene count];i++) {
        NSString *line = [_oblibene objectAtIndex:i];
        if ([line isEqual:[_dataPlayer valueForKey:@"id"]]){
            [_oblibene removeObjectAtIndex:i];
            isIn = YES;
            break;
        }
    }
    if (!isIn)
        [_oblibene addObject:(NSString*)[_dataPlayer valueForKey:@"id"]];
}

- (NSMutableArray*) getOblibeneRadia
{
    NSMutableArray *res = [[NSMutableArray alloc]init];
    
    for (int i=0; i<[_radioAll count];i=i+2){
        
        NSMutableDictionary* line = [_radioAll objectAtIndex:i];
        NSString *radId = [line valueForKey:@"id"];
        
        for (NSString *lineObl in _oblibene) {
            if ([lineObl isEqual:radId]) {
                [res addObject:[line copy]];
                [res addObject:[[_radioAll objectAtIndex:i+1]copy]];
            }
        }
        
    }
    return res;
}

#pragma mark Poslouchane
- (NSMutableArray*) getPoslouchaneRadia
{
    NSMutableArray *res = [[NSMutableArray alloc]init];
    //    for (NSString *lineObl in _poslouchane) {
    for (int y=[_poslouchane count]-1; y>=0;y--) {
        NSString *lineObl = [_poslouchane objectAtIndex:y];
        for (int i=0; i<[_radioAll count];i=i+2){
            NSMutableDictionary* line = [_radioAll objectAtIndex:i];
            if ([lineObl isEqual:[line valueForKey:@"id"]]) {
                [res addObject:[line copy]];
                [res addObject:[[_radioAll objectAtIndex:i+1]copy]];
            }
        }
        
    }
    return res;
}


-(void)preparePlay:(comPlayer *)player
{
    _meta = @"";
    _artSONG = @"";
    __artIMG = @"";
    _artImages = [[NSMutableDictionary alloc] init];
    _artARTIST = @"";
    
    [_activity setHidden:NO];
    [_activity startAnimating];
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
    
    [self addListener];
    
}

-(void)startPlaying:(comPlayer *)player
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    
    [_activity setHidden:YES];
    [_activity stopAnimating];
}

-(void)stopPlaying:(comPlayer *)player
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    
    [_activity setHidden:YES];
    [_activity stopAnimating];
    
    _meta = @"";
    _artSONG = @"";
    __artIMG = @"";
    _artImages = [[NSMutableDictionary alloc] init];
    _artARTIST = @"";
    [self _showArt:nil];
    //    [self hrajeBtn];
    
}
-(void)stopInteruptPlaying:(comPlayer *)player{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
}
-(void)startInteruptPlaying:(comPlayer *)player{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}

-(void)metaData:(comPlayer *)player meta:(NSString *)meta_incom
{
    //    NSString *metastr = [timedMetadata stringValue];
    NSString *metastr = meta_incom;
    NSString *znak;
    NSString *metafinal = [[NSString alloc]init];
    
    for (int i=0;i<[metastr length];i++) {
        if ([metastr characterAtIndex:i]!=0xc3 && [metastr characterAtIndex:i] != 0xc2 && [metastr characterAtIndex:i] != 0xbe && [metastr characterAtIndex:i] != 0xc5 && [metastr characterAtIndex:i] != 0xc4){
            unichar okznak = [metastr characterAtIndex:i];
            znak = [NSString stringWithCharacters:&okznak length:1];
            metafinal = [metafinal stringByAppendingFormat:@"%@", znak];
        }else {
            if ([metastr length]>i+1){
                unichar strRidici = [metastr characterAtIndex:i];
                i++;
                unichar strZnak = [metastr characterAtIndex:i];
                switch (strRidici) {
                    case 195:
                        switch (strZnak) {
                            case 173:
                                znak = @"í";
                                break;
                            case 161:
                                znak = @"á";
                                break;
                            case 169:
                                znak = @"é";
                                break;
                            case 189:
                                znak = @"ý";
                                break;
                            case 186:
                                znak = @"ú";
                                break;
                            case 157:
                                znak = @"Ý";
                                break;
                            case 129:
                                znak = @"Á";
                                break;
                            case 141:
                                znak = @"Í";
                                break;
                            case 137:
                                znak = @"É";
                                break;
                            case 154:
                                znak = @"Ú";
                                break;
                            case 179:
                                znak = @"ó";
                                break;
                            case 147:
                                znak = @"Ó";
                                break;
                            case 130:
                                znak = @"'";
                                break;
                                
                            default:
                                znak = [NSString stringWithFormat:@"%d-%d", strRidici, strZnak];
                                break;
                        }
                        break;
                    case 197:
                        switch (strZnak) {
                            case 174:
                                znak = @"Ů";
                                break;
                            case 189:
                                znak = @"Ž";
                                break;
                            case 160:
                                znak = @"Š";
                                break;
                            case 161:
                                znak = @"š";
                                break;
                            case 153:
                                znak = @"ř";
                                break;
                            case 190:
                                znak = @"ž";
                                break;
                            case 175:
                                znak = @"ů";
                                break;
                            case 152:
                                znak = @"Ř";
                                break;
                            case 165:
                                znak = @"ť";
                                break;
                            case 164:
                                znak = @"Ť";
                                break;
                            case 136:
                                znak = @"ň";
                                break;
                            case 135:
                                znak = @"Ň";
                                break;
                                
                                
                            default:
                                znak = [NSString stringWithFormat:@"%d-%d", strRidici, strZnak];
                                break;
                        }
                        break;
                    case 196:
                        switch (strZnak) {
                            case 0xa1:
                                znak = @"š";
                                break;
                            case 0xaf:
                                znak = @"ů";
                                break;
                            case 0x8c:
                                znak = @"Č";
                                break;
                            case 0x8d:
                                znak = @"č";
                                break;
                            case 0x99:
                                znak = @"ř";
                                break;
                            case 165:
                                znak = @"ť";
                                break;
                            case 155:
                                znak = @"ě";
                                break;
                            case 154:
                                znak = @"Ě";
                                break;
                            case 143:
                                znak = @"ď";
                                break;
                            case 142:
                                znak = @"Ď";
                                break;
                            case 190:
                                znak = @"ľ";
                                break;
                            case 189:
                                znak = @"Ľ";
                                break;
                                
                            default:
                                znak = [NSString stringWithFormat:@"%d-%d", strRidici, strZnak];
                                break;
                        }
                        break;
                        
                    default:
                        // neni definovan
                        znak = [NSString stringWithFormat:@"%d-%d", strRidici, strZnak];
                        break;
                }
                
                metafinal = [metafinal stringByAppendingFormat:@"%@", znak];
            }
        }
    }
    
    _meta = [metafinal copy];
    
    if (_description)
        _description.text = [self meta];
    
    [self doArtwork];
}

- (void)playStart
{
    if ((([[Reachability reachabilityForLocalWiFi] currentReachabilityStatus] == ReachableViaWiFi)) || ([_defaults boolForKey:@"only_wifi"] != YES))
    {
        // zakazani stimace
        if ([_defaults boolForKey:@"dimmer_on_play"]==NO)
            [UIApplication sharedApplication].idleTimerDisabled = YES;
        
        if ([_defaults boolForKey:@"no_hide"]==NO)
            [[UIDevice currentDevice] setProximityMonitoringEnabled:YES];
        
        // zobrazeni prace na pozadi
        [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
        
        // Vyber streamu
        int URL3G = 0;
        int URLWiFi = 0;
        for (int i=0; i<[_streamPlayer count]; i++) {
            NSString *stream3gState = [[_streamPlayer objectAtIndex:i] valueForKey:@"def3g"];
            
            stream3gState = [stream3gState stringByReplacingOccurrencesOfString:@"\n" withString:@""];
            stream3gState = [stream3gState stringByReplacingOccurrencesOfString:@"\t" withString:@""];
            
            NSString *streamWiFiState = [[_streamPlayer objectAtIndex:i] valueForKey:@"defWifi"];
            streamWiFiState = [streamWiFiState stringByReplacingOccurrencesOfString:@"\n" withString:@""];
            streamWiFiState = [streamWiFiState stringByReplacingOccurrencesOfString:@"\t" withString:@""];
            
            if ([stream3gState isEqualToString:@"1"])
                URL3G = i;
            if ([streamWiFiState isEqualToString:@"1"])
                URLWiFi = i;
        }
        NSString *url;
        if (([[Reachability reachabilityForLocalWiFi] currentReachabilityStatus] == ReachableViaWiFi)){
            url = [[_streamPlayer objectAtIndex:URLWiFi] valueForKey:@"url"];
        }else {
            url = [[_streamPlayer objectAtIndex:URL3G] valueForKey:@"url"];
        }
        id path = [url stringByReplacingOccurrencesOfString:@"\n" withString:@""];
        path = [path stringByReplacingOccurrencesOfString:@"\t" withString:@""];
        
        
        NSString *pageviewName = @"Start radia:";
        [self.tracker set:pageviewName value:path];
        
        _playerClass.streamURL = path;
        [_playerClass play];
        
    }else {
        UIAlertView *alertView = [[UIAlertView alloc]
                                  initWithTitle:NSLocalizedString(@"only_wifi_title", nil)
                                  message:NSLocalizedString(@"wifi_not_alowed",nil)
                                  delegate:nil
                                  cancelButtonTitle:NSLocalizedString(@"ok_btn", nil)
                                  otherButtonTitles:nil];
        
        [alertView show];
    }
    
    [self _showArt:nil];
}

-(float)getRate{
    return _playerClass.rate;
}

- (void) playStop
{
    [_playerClass stop];
    
    [[UIDevice currentDevice] setProximityMonitoringEnabled:NO];
    
    [UIApplication sharedApplication].idleTimerDisabled = NO;
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
        
    NSString *pageviewName = @"Stop radia";
    [self.tracker set:pageviewName value:@""];
    //    [self hrajeBtn];
    [_activity setHidden:YES];
    [_activity stopAnimating];
    
    _meta = @"";
    _artSONG = @"";
    __artIMG = @"";
    _artImages = [[NSMutableDictionary alloc] init];
    _artARTIST = @"";
    [self _showArt:nil];
}

- (void) sleepTouch
{
    _sleepTimeLeft +=15;
    if (_sleepTimeLeft>60)
        _sleepTimeLeft=0;
    
    if (_sleepTimeLeft>0)
    {
        if (!_sleepTimer) {
            _sleepTimer = [NSTimer scheduledTimerWithTimeInterval:60 target:self selector:@selector(testTimer:) userInfo:nil repeats:YES];
        }
    }else if (_sleepTimer) {
        [_sleepTimer invalidate];
        _sleepTimer = nil;
    }
    
}

-(void) addListener
{
    NSString *id_radia = [_dataPlayer valueForKey:@"id"];
    
    if (![_defaults boolForKey:@"listeners_all"]){
        //vymaz jiz zadaneho radia
        for (int i=0; i<[_poslouchane count];i++) {
            NSString *id_in_list = [_poslouchane objectAtIndex:i];
            if ([id_in_list isEqualToString:id_radia]) {
                [_poslouchane removeObjectAtIndex:i];
            }
        }
    }
    
    if ([_poslouchane count]>=[_defaults integerForKey:@"listeners_max"]){
        while ([_poslouchane count]>=[_defaults integerForKey:@"listeners_max"]) {
            [_poslouchane removeObjectAtIndex:[_poslouchane count]-1];
        }
        
    }
    
    [_poslouchane addObject:id_radia];
}

//-(void) doShowPlayer:(bool)startPlay :(id)sender;
//{
//    NSString *pageviewName = @"Detail:";
//    pageviewName = [pageviewName stringByAppendingString:@"player"];
//
//    [[GANTracker sharedTracker] trackPageview:pageviewName withError:nil];
//    if ([GANTracker sharedTracker].debug) {
//        NSLog(@"Dispatched %@ pageview for tracked view controller %@",
//              pageviewName, NSStringFromClass([self class]));
//    }
//
//    if (startPlay)
//        [_playerView play:0];
//}
//
//-(void)showPlayer
//{
//    [self.tabBarController setSelectedIndex:0];
//    if ([[_navController1 visibleViewController] isEqual:_playerView])
//    {
//        [_playerView dismissViewControllerAnimated:NO completion:^{
//            [self doShowPlayer:YES :0];
//        }];
//        [_playerView play:0];
//    } else {
//        [self doShowPlayer:YES :0];
//    }
//}
//
//- (void) btnHraje:(id)sender
//{
//    [self.tabBarController setSelectedIndex:0];
//    if ([[_navController1 visibleViewController] isEqual:_playerView])
//    {
//        [_playerView dismissViewControllerAnimated:NO completion:^{
//            [self doShowPlayer:NO :sender];
//        }];
//    } else {
//        [self doShowPlayer:NO :sender];
//    }
//}
//
//-(BOOL)isPlayerInPopover
//{
//    return [_playerView isModalInPopover];
//}

- (void) cancelSearch;
{
    self.categoryAktual = [_category copy];
    
    
    _radioAll = [[NSMutableArray alloc]init];
    for (int i=1;i<[self.categoryAktual count];i=i+2){
        NSMutableArray *radia = [self.categoryAktual objectAtIndex:i];
        for (int ii=0;ii<[radia count];ii=ii+2){
            [_radioAll addObject:[radia objectAtIndex:ii]];
            [_radioAll addObject:[radia objectAtIndex:ii+1]];
        }
    }
}

- (void) setSearch:(NSString*)search;
{
    self.categoryAktual = [[NSMutableArray alloc] init];
    
    int c=0;
    for (int i=0; i<[_category count]; i=i+2) {
        BOOL addCat = NO;
        NSMutableDictionary *cat = [_category objectAtIndex:i];
        NSString *scan = [NSString stringWithString:[cat valueForKey:@"title"]];
        NSRange textRange;
        textRange =[scan rangeOfString:search  options:NSCaseInsensitiveSearch];
        if (textRange.location != NSNotFound){
            [self.categoryAktual insertObject:cat atIndex:c];
            c++;
            addCat = YES;
        }
        NSMutableArray *radDo = [[NSMutableArray alloc]init];
        NSMutableArray *rad = [_category objectAtIndex:i+1];
        int cc=0;
        for (int ii=0; ii<[rad count]; ii=ii+2) {
            NSMutableDictionary *radItem = [rad objectAtIndex:ii];
            NSString *scan = [NSString stringWithString:[radItem valueForKey:@"name"]];
            NSRange textRange;
            textRange =[scan rangeOfString:search  options:NSCaseInsensitiveSearch];
            if (textRange.location != NSNotFound){
                [radDo insertObject:[rad objectAtIndex:ii] atIndex:cc];
                cc++;
                [radDo insertObject:[rad objectAtIndex:ii+1] atIndex:cc];
                cc++;
            }
        }
        if ([radDo count]>0) {
            if (!addCat) {
                [self.categoryAktual insertObject:cat atIndex:c];
                c++;
            }
            [self.categoryAktual insertObject:radDo atIndex:c];
            c++;
        }
    }
    
    _radioAll = [[NSMutableArray alloc]init];
    for (int i=1;i<[self.categoryAktual count];i=i+2){
        NSMutableArray *radia = [self.categoryAktual objectAtIndex:i];
        for (int ii=0;ii<[radia count];ii=ii+2){
            [_radioAll addObject:[radia objectAtIndex:ii]];
            [_radioAll addObject:[radia objectAtIndex:ii+1]];
        }
    }
}

-(BOOL)tabBarController:(UITabBarController *)tabBarController shouldSelectViewController:(UIViewController *)viewController
{
    [self cancelSearch];
    
    return YES;
}

- (NSString*) getArtImage:(UIImageView*)image;
{
    BOOL onWiFi = [[Reachability reachabilityForLocalWiFi] currentReachabilityStatus] == ReachableViaWiFi;
    int height = image.bounds.size.height;
    
    if (onWiFi || [_defaults boolForKey:@"hi_art"]){
        // lepsi kvalita
        for (id key in _artImages) {
            NSString *img = [key stringByReplacingOccurrencesOfString:@"image_" withString:@""];
            if ([img intValue]>=height){
                __artIMG = [_artImages valueForKey:key];
            }
        }
    }else {
        // horsi kvalita
        for (id key in _artImages) {
            NSString *img = [key stringByReplacingOccurrencesOfString:@"image_" withString:@""];
            if ([img intValue]<=height){
                __artIMG = [_artImages valueForKey:key];
            }
        }
    }
    return __artIMG;
}

@end
