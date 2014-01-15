//
//  comAppDelegate.m
//  ABradio
//
//  Created by Jan Damek on /202/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "comAppDelegate.h"
#import "comCategoryViewController.h"
#import "comDetailViewController.h"
#import "AsyncImageView.h"
#import "Reachability.h"
#import "comData.h"
#import "XMLReader.h"
#import "comXMLParseAbRadio.h"

@interface comAppDelegate(){
    BOOL _isInSleep;
    int _sleepTimeLeft;
    NSTimer *_sleepTimer;
    UIBackgroundTaskIdentifier _backgroundTask;
    
    NSUserDefaults *_defaults;
    
    BOOL _isInLoadXML;
    
    comPlayer *_playerClass;
}

@end

@implementation comAppDelegate

@synthesize window = _window;

@synthesize isInSleep =_isInSleep;
@synthesize sleepTimeLeft = _sleepTimeLeft;

@synthesize artArtist = _artArtist;
@synthesize artSong = _artSong;

@synthesize data = _data;

@synthesize managedObjectContext = _managedObjectContext;
@synthesize managedObjectModel = _managedObjectModel;
@synthesize persistentStoreCoordinator = _persistentStoreCoordinator;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    _isInSleep = NO;
    _sleepTimeLeft = 0;

    _isInLoadXML = NO;
    
    _playerClass = [[comPlayer alloc]init];
    _playerClass.delegate = self;
    
    NSDictionary *userDefaultsDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                          @"20", @"listeners_max",
                                          nil];
    [[NSUserDefaults standardUserDefaults] registerDefaults:userDefaultsDefaults];
    
    _defaults = [NSUserDefaults standardUserDefaults];
       
    [[UIApplication sharedApplication] setStatusBarHidden:NO];
    
    // Override point for customization after application launch.
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        UISplitViewController *splitViewController = (UISplitViewController *)self.window.rootViewController;
        UINavigationController *navigationController = [splitViewController.viewControllers lastObject];
        splitViewController.delegate = (id)navigationController.topViewController;
    }
    return YES;
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
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [self.data saveStreams];
    [self.data saveRadios];
    [self.data saveCategories];
    
    [self.data saveFavorites];
    [self.data saveListeneds];
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

- (void)_showArt:(NSTimer *)timer
{
    NSString *praveHraje;
    if (![_artArtist isEqualToString:@""]){
        praveHraje = [NSString stringWithFormat:@"%@ - %@",_artArtist,_artSong];
    }
//    else {
//        praveHraje = [NSString stringWithFormat:@"%@",_meta];
//    }
    praveHraje = [praveHraje stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    praveHraje = [praveHraje stringByReplacingOccurrencesOfString:@"\t" withString:@""];
    
//    if (_artImageView){
//        NSString *imgURL;
//        if (![[self getArtImage:_artImageView]isEqualToString:@""]){
//            imgURL = [self getArtImage:_artImageView];
//            UIImage *image = [[[AsyncImageView alloc]init]getImage:imgURL list:_imageList imageView:_artImageView cell:nil];
//            [_artImageView setImage:image];
//        }else {
//            [_artImageView setImage:[UIImage imageNamed:@"vodoznak"]];
//        }
//    }
}

- (void)parseXMLFileAtURL:(NSString *)URL
{
    if (!_isInLoadXML)
    {
        _isInLoadXML = YES;
        
        NSString *agentString = @"ABRadio/1.1 (iPhone/iPad) AppleWebKit/525.27.1 (KHTML, like Gecko) Version/3.2.1 Safari/525.27.1";
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                        [NSURL URLWithString:URL]];
        [request setValue:agentString forHTTPHeaderField:@"User-Agent"];

        NSOperationQueue *queue = [[NSOperationQueue alloc] init];
            _isInLoadXML = YES;
            [NSURLConnection sendAsynchronousRequest:request queue:queue completionHandler:^(NSURLResponse *response, NSData *data, NSError *error)
             {
                 
                 if (!error && [data length] > 0)
                 {
                     NSDictionary *d= [XMLReader dictionaryForXMLData:data error:nil];
                     //TODO: parse data from XML
                     for (NSDictionary *item in d) {
                         //TODO: separate artist and song
                         _artArtist = [item objectForKey:@"ARTIST"];
                     }
                     
                 }
                 else
                 {
                     NSLog(@"error loading XML");
                 }
                 
                 _isInLoadXML = NO;
             }];
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
    comXMLParseAbRadio *abRadio = [[comXMLParseAbRadio alloc]init];
    [abRadio parseAbRadioXML];
}

-(void) doArtwork
{
    // provedeni parseru pro artwork k prehravaci
//    NSString *artworkURL = [_dataPlayer valueForKey:@"artwork"];
//    artworkURL = [artworkURL stringByReplacingOccurrencesOfString:@"\n" withString:@""];
//    artworkURL = [artworkURL stringByReplacingOccurrencesOfString:@"\t" withString:@""];
//    if (![artworkURL isEqualToString:@""]){
//        [self parseXMLFileAtURL:artworkURL];
//        
//    }
}

#pragma mark Oblibene

- (BOOL)isInOblibene
{
    BOOL ret = NO;
//    for (NSString *line in _oblibene) {
//        if ([line isEqual:[_dataPlayer valueForKey:@"id"]]){
//            ret = YES;
//            break;
//        }
//    }
    return ret;
}

- (void)togleInOblibene
{
//    BOOL isIn = NO;
//    for (int i=0;i<[_oblibene count];i++) {
//        NSString *line = [_oblibene objectAtIndex:i];
//        if ([line isEqual:[_dataPlayer valueForKey:@"id"]]){
//            [_oblibene removeObjectAtIndex:i];
//            isIn = YES;
//            break;
//        }
//    }
//    if (!isIn)
//        [_oblibene addObject:(NSString*)[_dataPlayer valueForKey:@"id"]];
}

- (NSMutableArray*) getOblibeneRadia
{
    NSMutableArray *res = [[NSMutableArray alloc]init];
    
//    for (int i=0; i<[_radioAll count];i=i+2){
//        
//        NSMutableDictionary* line = [_radioAll objectAtIndex:i];
//        NSString *radId = [line valueForKey:@"id"];
//        
//        for (NSString *lineObl in _oblibene) {
//            if ([lineObl isEqual:radId]) {
//                [res addObject:[line copy]];
//                [res addObject:[[_radioAll objectAtIndex:i+1]copy]];
//            }
//        }
//        
//    }
    return res;
}

#pragma mark Poslouchane
- (NSMutableArray*) getPoslouchaneRadia
{
    NSMutableArray *res = [[NSMutableArray alloc]init];
//    //    for (NSString *lineObl in _poslouchane) {
//    for (long y=[_poslouchane count]-1; y>=0;y--) {
//        NSString *lineObl = [_poslouchane objectAtIndex:y];
//        for (int i=0; i<[_radioAll count];i=i+2){
//            NSMutableDictionary* line = [_radioAll objectAtIndex:i];
//            if ([lineObl isEqual:[line valueForKey:@"id"]]) {
//                [res addObject:[line copy]];
//                [res addObject:[[_radioAll objectAtIndex:i+1]copy]];
//            }
//        }
//        
//    }
    return res;
}


-(void)preparePlay:(comPlayer *)player
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
    
    [self addListener];
}

-(void)startPlaying:(comPlayer *)player
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;

    _artArtist = @"";
    _artSong = @"";
    [self _showArt:nil];
}

-(void)stopPlaying:(comPlayer *)player
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    
    _artArtist = @"";
    _artSong = @"";
    [self _showArt:nil];
    
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
    
//    _meta = [metafinal copy];
//    
//    if (_description)
//        _description.text = [self meta];
    
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
//        int URL3G = 0;
//        int URLWiFi = 0;
//        for (int i=0; i<[_streamPlayer count]; i++) {
//            NSString *stream3gState = [[_streamPlayer objectAtIndex:i] valueForKey:@"def3g"];
//            
//            stream3gState = [stream3gState stringByReplacingOccurrencesOfString:@"\n" withString:@""];
//            stream3gState = [stream3gState stringByReplacingOccurrencesOfString:@"\t" withString:@""];
//            
//            NSString *streamWiFiState = [[_streamPlayer objectAtIndex:i] valueForKey:@"defWifi"];
//            streamWiFiState = [streamWiFiState stringByReplacingOccurrencesOfString:@"\n" withString:@""];
//            streamWiFiState = [streamWiFiState stringByReplacingOccurrencesOfString:@"\t" withString:@""];
//            
//            if ([stream3gState isEqualToString:@"1"])
//                URL3G = i;
//            if ([streamWiFiState isEqualToString:@"1"])
//                URLWiFi = i;
//        }
//        NSString *url;
//        if (([[Reachability reachabilityForLocalWiFi] currentReachabilityStatus] == ReachableViaWiFi)){
//            url = [[_streamPlayer objectAtIndex:URLWiFi] valueForKey:@"url"];
//        }else {
//            url = [[_streamPlayer objectAtIndex:URL3G] valueForKey:@"url"];
//        }
//        id path = [url stringByReplacingOccurrencesOfString:@"\n" withString:@""];
//        path = [path stringByReplacingOccurrencesOfString:@"\t" withString:@""];
        
        _playerClass.streamURL = @"";//path;
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
        
    
    _artSong = @"";
    _artArtist = @"";
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
//    NSString *id_radia = [_dataPlayer valueForKey:@"id"];
//    
//    if (![_defaults boolForKey:@"listeners_all"]){
//        //vymaz jiz zadaneho radia
//        for (int i=0; i<[_poslouchane count];i++) {
//            NSString *id_in_list = [_poslouchane objectAtIndex:i];
//            if ([id_in_list isEqualToString:id_radia]) {
//                [_poslouchane removeObjectAtIndex:i];
//            }
//        }
//    }
//    
//    if ([_poslouchane count]>=[_defaults integerForKey:@"listeners_max"]){
//        while ([_poslouchane count]>=[_defaults integerForKey:@"listeners_max"]) {
//            [_poslouchane removeObjectAtIndex:[_poslouchane count]-1];
//        }
//        
//    }
//    
//    [_poslouchane addObject:id_radia];
}

- (NSString*) getArtImage:(UIImageView*)image;
{
//    BOOL onWiFi = [[Reachability reachabilityForLocalWiFi] currentReachabilityStatus] == ReachableViaWiFi;
//    int height = image.bounds.size.height;
//    
//    if (onWiFi || [_defaults boolForKey:@"hi_art"]){
//        // lepsi kvalita
//        for (id key in _artImages) {
//            NSString *img = [key stringByReplacingOccurrencesOfString:@"image_" withString:@""];
//            if ([img intValue]>=height){
//                __artIMG = [_artImages valueForKey:key];
//            }
//        }
//    }else {
//        // horsi kvalita
//        for (id key in _artImages) {
//            NSString *img = [key stringByReplacingOccurrencesOfString:@"image_" withString:@""];
//            if ([img intValue]<=height){
//                __artIMG = [_artImages valueForKey:key];
//            }
//        }
//    }
    return nil;
}

- (void)saveContext
{
    NSError *error = nil;
    NSManagedObjectContext *managedObjectContext = self.managedObjectContext;
    if (managedObjectContext != nil) {
        if ([managedObjectContext hasChanges] && ![managedObjectContext save:&error]) {
            // Replace this implementation with code to handle the error appropriately.
            // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
}

#pragma mark - Core Data stack

// Returns the managed object context for the application.
// If the context doesn't already exist, it is created and bound to the persistent store coordinator for the application.
- (NSManagedObjectContext *)managedObjectContext
{
    if (_managedObjectContext != nil) {
        return _managedObjectContext;
    }
    
    NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
    if (coordinator != nil) {
        _managedObjectContext = [[NSManagedObjectContext alloc] init];
        [_managedObjectContext setPersistentStoreCoordinator:coordinator];
    }
    return _managedObjectContext;
}

// Returns the managed object model for the application.
// If the model doesn't already exist, it is created from the application's model.
- (NSManagedObjectModel *)managedObjectModel
{
    if (_managedObjectModel != nil) {
        return _managedObjectModel;
    }
    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"iRadio" withExtension:@"momd"];
    _managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    return _managedObjectModel;
}

// Returns the persistent store coordinator for the application.
// If the coordinator doesn't already exist, it is created and the application's store added to it.
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
    if (_persistentStoreCoordinator != nil) {
        return _persistentStoreCoordinator;
    }
    
    NSURL *storeURL = [[self applicationDocumentsDirectory] URLByAppendingPathComponent:@"iRadio.sqlite"];
    
    NSError *error = nil;
    _persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:[self managedObjectModel]];
    if (![_persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:storeURL options:@{NSMigratePersistentStoresAutomaticallyOption:@YES, NSInferMappingModelAutomaticallyOption:@YES} error:&error]) {
        /*
         Replace this implementation with code to handle the error appropriately.
         
         abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development.
         
         Typical reasons for an error here include:
         * The persistent store is not accessible;
         * The schema for the persistent store is incompatible with current managed object model.
         Check the error message to determine what the actual problem was.
         
         
         If the persistent store is not accessible, there is typically something wrong with the file path. Often, a file URL is pointing into the application's resources directory instead of a writeable directory.
         
         If you encounter schema incompatibility errors during development, you can reduce their frequency by:
         * Simply deleting the existing store:
         */
        
        [[NSFileManager defaultManager] removeItemAtURL:storeURL error:nil];
        
        /*
         * Performing automatic lightweight migration by passing the following dictionary as the options parameter:
         @{NSMigratePersistentStoresAutomaticallyOption:@YES, NSInferMappingModelAutomaticallyOption:@YES}
         
         Lightweight migration will only work for a limited set of schema changes; consult "Core Data Model Versioning and Data Migration Programming Guide" for details.
         
         */
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
    
    return _persistentStoreCoordinator;
}

#pragma mark - Application's Documents directory

// Returns the URL to the application's Documents directory.
- (NSURL *)applicationDocumentsDirectory
{
    return [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
}

-(comData *)getData{
    if (!_data){
        _data = [[comData alloc] init];
    }
    return _data;
}

@end
