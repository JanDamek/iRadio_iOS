//
//  comAppDelegate.h
//  ABradio
//
//  Created by Jan Damek on /202/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
//#import <AVFoundation/AVFoundation.h>
//#import <AudioToolbox/AudioToolbox.h>
//#import <MediaPlayer/MPMusicPlayerController.h>
#import "comPlayer.h"
#import "comWebViewController.h"
#import "GADBannerView.h"
#import "comListenViewController.h"
#import "comKategorieListViewController.h"
#import "comFavoriteViewController.h"
#import "comPlayerViewController.h"
#import "GAITracker.h"

@protocol UpdateABRadio
-(void)updateTableView;
-(void)updateActualSong;
@end

@interface comAppDelegate : UIResponder <UIApplicationDelegate, UITabBarControllerDelegate, NSXMLParserDelegate, comPlayerDelegate>{
    
    NSString *_currentElement;
    NSMutableString *_ElementValue;
    BOOL _errorParsing;
    NSXMLParser *_rssParser;
    
    NSMutableArray *_category;
    NSMutableDictionary *_itemCat;    
    BOOL _cat;

    NSMutableArray *_radio;
    NSMutableArray *_radioAll;
    NSMutableDictionary *_itemRadio;
    BOOL _rad;

    NSMutableArray *_stream;
    NSMutableDictionary *_itemStream;
    BOOL _stre;
    
    comPlayer *_playerClass;
    
    NSMutableDictionary *_imageList;    

    NSMutableDictionary *_dataPlayer;
    NSMutableArray *_streamPlayer;

    UITextView *_description;
    UIImageView* _artImageView;
    NSString* _meta;
    NSString* __weak _artSONG;
    NSString* __weak _artARTIST;
    NSString* __artIMG;
    NSMutableDictionary* _artImages;
    UIActivityIndicatorView *_activity;
    UILabel * _sleepLabel;
    
    NSMutableArray* _oblibene;
    NSMutableArray* _poslouchane; 
    
    BOOL _isInSleep;
    int _sleepTimeLeft;
    NSTimer *_sleepTimer;
    UIBackgroundTaskIdentifier _backgroundTask; 
    
   
    NSUserDefaults *_defaults;   
    
    UIActivityIndicatorView *_ani;
    
    BOOL _isInLoadXML;
    
    unsigned int radioList;
}

@property (readwrite) unsigned int radioList;

@property (strong, nonatomic) UIWindow *window;

@property (strong, nonatomic) UITextView *description;
@property (readonly, getter = getRate) float rate;
@property (readonly) NSMutableDictionary *imageList;
@property (strong, nonatomic) NSMutableDictionary *dataPlayer;
@property (strong, nonatomic) NSMutableArray *streamPlayer;
@property (weak, readonly) NSString* artSONG;
@property (weak, readonly) NSString* artARTIST;
@property (strong, nonatomic) UIImageView *artImageView;
@property (strong, nonatomic) UIActivityIndicatorView *activity;
@property (readonly) NSMutableArray *oblibene;
@property (readonly) NSMutableArray *poslouchane;
@property (readwrite) BOOL isInSleep;
@property (readwrite) int sleepTimeLeft;
@property (strong, nonatomic) UILabel *sleepLabel;
@property (readwrite) int cntBaner;
@property (strong, nonatomic) NSString *requestURL;
@property (strong, nonatomic) UITableView *table;
@property BOOL clicked;
@property (nonatomic, strong) id<GAITracker> tracker;
@property (nonatomic, strong) NSMutableArray *categoryAktual;

+ (GADBannerView*) getBanner;
//- (NSMutableArray*) getCategory;
- (void)parseXMLFileAtURL:(NSString *)URL;

void MyImgLoaded (UIImage* image, NSString* path);

- (void)playStart;
- (void)playStop;

- (BOOL)isInOblibene;
- (void)togleInOblibene;
- (void)sleepTouch;

- (NSMutableArray*) getOblibeneRadia;
- (NSMutableArray*) getPoslouchaneRadia;

- (NSString*) prg;
- (NSString*) meta;

//- (void) showPlayer;

//- (BOOL) isPlayerInPopover;

- (void) cancelSearch;
- (void) setSearch:(NSString*)search;

- (NSString*) getArtImage:(UIImageView*)image;

-(void) parseXMLCategorie;
@end
