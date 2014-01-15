//
//  comPlayer.m
//  ABradio
//
//  Created by Jan Damek on 28.02.13.
//  Copyright (c) 2013 droidsoft.eu. All rights reserved.
//

#import "comPlayer.h"

@interface comPlayer (){
    
    BOOL _regTimedMeta;
    AVPlayer *_player;
    AVPlayerItem *_playerItem;
}
-(void)prepareBackgroundPlayAndSetDefaultSoundRoute;
@end

@interface comPlayer (Player)
- (void)handleTimedMetadata:(AVMetadataItem*)timedMetadata;
- (void)assetFailedToPrepareForPlayback:(NSError *)error;
- (void)prepareToPlayAsset:(AVURLAsset *)asset withKeys:(NSArray *)requestedKeys;
@end


@implementation comPlayer

@synthesize delegate = _delegate;
@synthesize streamURL = _streamURL;
@synthesize player = _player;
@synthesize lastMetaData = _lastMetaData;

static void *comSecondViewControllerCurrentItemObservationContext = &comSecondViewControllerCurrentItemObservationContext;
static void *comSecondViewControllerPlayerItemStatusObserverContext = &comSecondViewControllerPlayerItemStatusObserverContext;
static void *comSecondViewControllerTimedMetadataObserverContext = &comSecondViewControllerTimedMetadataObserverContext;

NSString *kCurrentItemKey	= @"currentItem";
NSString *kStatusKey		= @"status";
NSString *kTimedMetadataKey	= @"timedMetadata";
NSString *kTracksKey		= @"tracks";
NSString *kPlayableKey		= @"playable";

- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player successfully:(BOOL)flag
{
    if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
        [self.delegate stopPlaying:self];
    }
}
- (void)audioPlayerDecodeErrorDidOccur:(AVAudioPlayer *)player error:(NSError *)error
{
    if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
        [self.delegate stopPlaying:self];
    }
}
- (void)audioPlayerBeginInterruption:(AVAudioPlayer *)player
{
    if ([self.delegate respondsToSelector:@selector(startInteruptPlaying:)]) {
        [self.delegate startInteruptPlaying:self];
    }
}
- (void)audioPlayerEndInterruption:(AVAudioPlayer *)player withOptions:(NSUInteger)flags NS_AVAILABLE_IOS(6_0){
    if ([self.delegate respondsToSelector:@selector(stopInteruptPlaying:)]) {
        [self.delegate stopInteruptPlaying:self];
    }
};
- (void)audioPlayerEndInterruption:(AVAudioPlayer *)player withFlags:(NSUInteger)flags NS_DEPRECATED_IOS(4_0, 6_0){
    if ([self.delegate respondsToSelector:@selector(stopInteruptPlaying:)]) {
        [self.delegate stopInteruptPlaying:self];
    }
};
- (void)audioPlayerEndInterruption:(AVAudioPlayer *)player NS_DEPRECATED_IOS(2_2, 6_0){
    if ([self.delegate respondsToSelector:@selector(stopInteruptPlaying:)]) {
        [self.delegate stopInteruptPlaying:self];
    }
};

-(id)init{
    if (self=[super init]){
        [self prepareBackgroundPlayAndSetDefaultSoundRoute];
    }
    
    _streamURL = @"";
    _playerItem = nil;
    _player = nil;
    return self;
}

-(id)initWithURL:(NSString *)streamURL
{
    if (self = [super init]){
        _streamURL = streamURL;
        [self prepareBackgroundPlayAndSetDefaultSoundRoute];
    } else {
        _streamURL = @"";
    }
    
    _playerItem = nil;
    _player = nil;
    return self;
}

-(void)prepareBackgroundPlayAndSetDefaultSoundRoute
{
    //get your app's audioSession singleton object
    AVAudioSession* session = [AVAudioSession sharedInstance];
    
    //error handling
    BOOL success;
    NSError* error;
    
    //set the audioSession category.
    //Needs to be Record or PlayAndRecord to use audioRouteOverride:
    
    success = [session setCategory:AVAudioSessionCategoryPlayback error:&error];
    
    if (!success)
        NSLog(@"AVAudioSession error setting category:%@",error);
    
    //set the audioSession override
    success = [session overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker
                                         error:&error];
    if (!success)
        NSLog(@"AVAudioSession error overrideOutputAudioPort:%@",error);
    
    //activate the audio session
    success = [session setActive:YES error:&error];
    if (!success)
        NSLog(@"AVAudioSession error activating: %@",error);
    else
        NSLog(@"audioSession active");    
}

-(void)play
{
    NSURL *newMovieURL = [NSURL URLWithString:_streamURL];
    if ([newMovieURL scheme])	// test na pravost url
    {
        
        // vytvoreni URLAsset pro klice "tracks", "playable".
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:newMovieURL options:nil];
        NSArray *requestedKeys = [NSArray arrayWithObjects:kTracksKey, kPlayableKey, nil];
        
        /* natazeni klicu to asset, pokud uz neni nahrany. */
        [asset loadValuesAsynchronouslyForKeys:requestedKeys completionHandler:
         ^{
             dispatch_async( dispatch_get_main_queue(),
                            ^{
                                /* priprava prehravace */
                                [self prepareToPlayAsset:asset withKeys:requestedKeys];
                                
                                if ([self.delegate respondsToSelector:@selector(preparePlay:)]) {
                                    [self.delegate preparePlay:self];
                                }
                                
                            });
         }];
        
    }
}

-(void)stop
{
    [_player pause];
    
    if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
        [self.delegate stopPlaying:self];
    }
}

-(float)getRate{
    return [_player rate];
}

- (void)handleTimedMetadata:(AVMetadataItem*)timedMetadata
{
    _lastMetaData = [timedMetadata stringValue];
    
    if ([self.delegate respondsToSelector:@selector(metaData:meta:)]) {
        [self.delegate metaData:self meta:_lastMetaData];
    }
}

-(void)assetFailedToPrepareForPlayback:(NSError *)error
{
    if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
        [self.delegate stopPlaying:self];
    }
}

- (void)playerItemDidReachEnd:(NSNotification *)notification{
    if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
        [self.delegate stopPlaying:self];
    }
}

- (void)prepareToPlayAsset:(AVURLAsset *)asset withKeys:(NSArray *)requestedKeys
{
	for (NSString *thisKey in requestedKeys)
    {
		NSError *error = nil;
		AVKeyValueStatus keyStatus = [asset statusOfValueForKey:thisKey error:&error];
		if (keyStatus == AVKeyValueStatusFailed)
        {
			[self assetFailedToPrepareForPlayback:error];
			return;
        }
    }
    
    // nelze prehravat
    if (!asset.playable)
    {
		NSString *localizedDescription = NSLocalizedString(@"Item cannot be played", @"Item cannot be played description");
		NSString *localizedFailureReason = NSLocalizedString(@"The assets tracks were loaded, but could not be made playable.", @"Item cannot be played failure reason");
		NSDictionary *errorDict = [NSDictionary dictionaryWithObjectsAndKeys:
								   localizedDescription, NSLocalizedDescriptionKey,
								   localizedFailureReason, NSLocalizedFailureReasonErrorKey,
								   nil];
		NSError *assetCannotBePlayedError = [NSError errorWithDomain:@"player" code:0 userInfo:errorDict];
        
        [self assetFailedToPrepareForPlayback:assetCannotBePlayedError];
        
        return;
    }
	
    // existujici polozka prehravace
    if (_playerItem)
    {
        // je ji nutno vyradit ze sledovani
        if (_regTimedMeta){
            [_player.currentItem removeObserver:self forKeyPath:kTimedMetadataKey];
            _regTimedMeta = NO;
        }
        
        [_playerItem removeObserver:self forKeyPath:kStatusKey];
		
        [[NSNotificationCenter defaultCenter] removeObserver:self
                                                        name:AVPlayerItemDidPlayToEndTimeNotification
                                                      object:_playerItem];
    }
	
    // vytvorit novou plozku prehravace
    _playerItem = [AVPlayerItem playerItemWithAsset:asset];
    
    //zarazeni k sledovani stavu
    [_playerItem addObserver:self
                  forKeyPath:kStatusKey
                     options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                     context:comSecondViewControllerPlayerItemStatusObserverContext];
	
    //zarazeni k sledovani konce
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(playerItemDidReachEnd:)
                                                 name:AVPlayerItemDidPlayToEndTimeNotification
                                               object:_playerItem];
	
    //pokud neni prehravac tak zalozit novy
    if (!_player)
    {
        // vyrvoreni noveho prehravace z polozky
        _player = [AVPlayer playerWithPlayerItem:_playerItem];
		
        //sledovani pro aktualni polozku
        [_player addObserver:self
                  forKeyPath:kCurrentItemKey
                     options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                     context:comSecondViewControllerCurrentItemObservationContext];
        
    }
    
    // pokud neni aktualni polozka polozkou k prehravani
    if (![_player.currentItem isEqual:_playerItem])
    {
        //vymenit aktualni polozku
        [_player replaceCurrentItemWithPlayerItem:_playerItem];
    }
    
}

- (void)observeValueForKeyPath:(NSString*) path
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context
{
    // zmena stavu prehravace
	if (context == comSecondViewControllerPlayerItemStatusObserverContext)
    {
        
        AVPlayerStatus status = [[change objectForKey:NSKeyValueChangeNewKey] integerValue];
        switch (status)
        {
                
            case AVPlayerStatusUnknown:
            {
                
                if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
                    [self.delegate stopPlaying:self];
                }
                
            }
                break;
                
            case AVPlayerStatusReadyToPlay:
            {
                // prehravac je pripraven a nabuffrovan
                
                [_player play];
                
                if ([self.delegate respondsToSelector:@selector(startPlaying:)]) {
                    [self.delegate startPlaying:self];
                }
                
                //sledovani metatagu
                [_player.currentItem addObserver:self
                                      forKeyPath:kTimedMetadataKey
                                         options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                                         context:comSecondViewControllerTimedMetadataObserverContext];
                _regTimedMeta = YES;
                
            }
                break;
                
            case AVPlayerStatusFailed:
            {
                // nastala chyba v inicializaci, nebo streamu
                
                AVPlayerItem *thePlayerItem = (AVPlayerItem *)object;
                [self assetFailedToPrepareForPlayback:thePlayerItem.error];
                
                if ([self.delegate respondsToSelector:@selector(stopPlaying:)]) {
                    [self.delegate stopPlaying:self];
                }
            }
                break;
        }
    }
	
    //zmena polozky pro prehravani
	else if (context == comSecondViewControllerCurrentItemObservationContext)
    {
        AVPlayerItem *newPlayerItem = [change objectForKey:NSKeyValueChangeNewKey];
        
        // neni polozka k prehravani
        if (newPlayerItem == (id)[NSNull null])
        {
        }
        else
        {
            
        }
        
    }
	
    // dorazili timetMetaData provede pro vseechny key volani handle
	else if (context == comSecondViewControllerTimedMetadataObserverContext)
    {
		NSArray* array = [[_player currentItem] timedMetadata];
		for (AVMetadataItem *metadataItem in array)
        {
			[self handleTimedMetadata:metadataItem];
        }
    }
	else
    {
		[super observeValueForKeyPath:path ofObject:object change:change context:context];
    }
    
    return;
}


@end
