//
//  comAppDelegate.h
//  ABradio
//
//  Created by Jan Damek on /202/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "comPlayer.h"
#import "GADBannerView.h"
#import "comData.h"

@interface comAppDelegate : UIResponder <UIApplicationDelegate, comPlayerDelegate>

@property (nonatomic, strong, getter = getData) comData* data;

@property (strong, nonatomic) UIWindow *window;

@property (readwrite) BOOL isInSleep;
@property (readwrite) int sleepTimeLeft;

@property (nonatomic, weak) NSString *artArtist;
@property (nonatomic, weak) NSString *artSong;

- (void)parseXMLFileAtURL:(NSString *)URL;

- (void)playStart;
- (void)playStop;

- (void)sleepTouch;

- (NSString*) getArtImage:(UIImageView*)image;

-(void) parseXMLCategorie;

@property (readonly, strong, nonatomic) NSManagedObjectContext *managedObjectContext;
@property (readonly, strong, nonatomic) NSManagedObjectModel *managedObjectModel;
@property (readonly, strong, nonatomic) NSPersistentStoreCoordinator *persistentStoreCoordinator;

- (void)saveContext;
- (NSURL *)applicationDocumentsDirectory;

@end
