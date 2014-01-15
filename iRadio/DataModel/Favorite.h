//
//  Favorite.h
//  iRadioHD
//
//  Created by Jan Damek on 15.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Radio;

@interface Favorite : NSManagedObject

@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) Radio *radios_rel;

@end
