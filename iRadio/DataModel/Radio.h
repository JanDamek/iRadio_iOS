//
//  Radio.h
//  iRadioHD
//
//  Created by Jan Damek on 15.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Categorie, Favorite, Listened, Stream;

@interface Radio : NSManagedObject

@property (nonatomic, retain) NSString * artwork;
@property (nonatomic, retain) NSString * descript;
@property (nonatomic, retain) NSString * interpret;
@property (nonatomic, retain) NSString * logo;
@property (nonatomic, retain) NSString * logo_ipad;
@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSString * program;
@property (nonatomic, retain) NSNumber * radio_id;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSDate * updated;
@property (nonatomic, retain) NSNumber * user_def;
@property (nonatomic, retain) Categorie *categorie_rel;
@property (nonatomic, retain) Favorite *favorited_rel;
@property (nonatomic, retain) Listened *listened_rel;
@property (nonatomic, retain) NSSet *stream_rel;
@end

@interface Radio (CoreDataGeneratedAccessors)

- (void)addStream_relObject:(Stream *)value;
- (void)removeStream_relObject:(Stream *)value;
- (void)addStream_rel:(NSSet *)values;
- (void)removeStream_rel:(NSSet *)values;

@end
