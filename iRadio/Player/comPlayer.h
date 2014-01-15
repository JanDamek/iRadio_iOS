//
//  comPlayer.h
//  ABradio
//
//  Created by Jan Damek on 28.02.13.
//  Copyright (c) 2013 droidsoft.eu. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <MediaPlayer/MPMusicPlayerController.h>

@class comPlayer;

@protocol comPlayerDelegate <NSObject>
@optional
-(void)metaData:(comPlayer*)player meta:(NSString*)meta;
-(void)stopPlaying:(comPlayer*)player;
-(void)startPlaying:(comPlayer*)player;
-(void)preparePlay:(comPlayer*)player;
-(void)startInteruptPlaying:(comPlayer*)player;
-(void)stopInteruptPlaying:(comPlayer*)player;
@end

@interface comPlayer : NSObject <AVAudioPlayerDelegate> 

@property (strong, atomic) id<comPlayerDelegate> delegate;
@property (copy) NSString *streamURL;
@property (readonly, getter = getRate) float rate;
@property (readonly) AVPlayer *player;
@property (readonly) NSString *lastMetaData;

-(id)initWithURL:(NSString *)streamURL;

-(void)play;
-(void)stop;
@end;